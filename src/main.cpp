#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <thread>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include "uDataPacketCacheService/service.hpp"
#include "uDataPacketCacheService/subscriber.hpp"
#include "uDataPacketCacheService/metricsSingleton.hpp"
#include "uDataPacketCacheService/utilities.hpp"
#include "programOptions.hpp"
#include "logger.hpp"
#include "metrics.hpp"

using namespace UDataPacketCacheService;

namespace
{

volatile std::sig_atomic_t mSignalStatus;
std::atomic_bool mInterrupted{false};

}

class Process
{
public:
    Process(::ProgramOptions &&options,
            std::shared_ptr<spdlog::logger> logger) :
        mOptions(std::move(options)),
        mLogger(std::move(logger))
    {
        mMaximumImportQueueSize = mOptions.maximumImportQueueSize;
        mImportQueue.set_capacity(mMaximumImportQueueSize);

        mDataPacketSubscriber
            = std::make_unique<Subscriber> (
                mOptions.dataPacketSubscriberOptions,
                mAddPacketCallback,
                mLogger);
    }

    /// @brief Destructor
    ~Process()
    {
        stop();
    }

    /// @brief Stops processes
    void stop()
    {
        if (mIsRunning)
        {
            mKeepRunning.store(false);
            mIsRunning = false;
            // Stop the import first
            if (mDataPacketSubscriber != nullptr)
            {
                mDataPacketSubscriber->stop();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
            // Now boot the subscribers
            if (mService != nullptr)
            {
                mService->stop();
             }
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
        }
    }

    /// Allows import thread to add packets
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        if (!packet.has_stream_identifier())
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Packet does not have identifier - skipping");
            return;
        }
        if (!packet.has_sampling_rate())
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Packet does not have sampling rate - skipping");
            return;
        }
        if (packet.sampling_rate() <= 0)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Sampling rate is invalid - skipping");
            return;
        }
        if (!packet.has_data_type())
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Packet does not have a data type - skipping");
            return;
        }
        if (!packet.has_number_of_samples())
        {
            SPDLOG_LOGGER_WARN(mLogger,
                         "Packet does not have a number of samples - skipping");
            return;
        }
        if (packet.number_of_samples() < 1)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Packet does is empty - skipping");
            return;
        }
        if (!packet.has_data())
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Packet does not data - skipping");
            return;
        }
        // Make sure there's space in the queue
        if (static_cast<size_t> (mImportQueue.size()) >= 
            mMaximumImportQueueSize)
        {
            SPDLOG_LOGGER_WARN(mLogger, "Queue full - popping packets");
            while (static_cast<size_t> (mImportQueue.size()) >=
                   mMaximumImportQueueSize)
            {
                UDataPacketServiceAPI::V1::Packet work;
                if (!mImportQueue.try_pop(work))
                {
                    SPDLOG_LOGGER_WARN(mLogger, "Failed to pop front of queue");
                    break;
                }
            }   
        }
        // Add it
        if (!mImportQueue.try_push(std::move(packet)))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to enqueue packet");
        }
    }

    /// Propagate packets to the backend
    void processPackets()
    {
        auto maximumLatency
            = std::chrono::nanoseconds{mOptions.maximumPacketLatency};
        constexpr std::chrono::milliseconds timeOut{10};
        while (mKeepRunning.load())
        {
            UDataPacketServiceAPI::V1::Packet packet;
            auto gotPacket = mImportQueue.try_pop(packet);
            if (gotPacket)
            {
                // Check the packet
                if (!packet.has_stream_identifier())
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                       "Packet does not have identifier - skipping");
                    continue;
                }
                std::string name;
                try
                {
                    name = Utilities::toString(packet.stream_identifier());
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Could not determine input packet name because {}",
                        std::string {e.what()});
                    continue;
                }
                if (!packet.has_sampling_rate())
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Packet for {} does not have sampling rate - skipping",
                        name);
                    continue;
                }
                if (!packet.has_start_time())
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Packet for {} does not have start time - skipping",
                        name);
                    continue;
                }
                if (packet.sampling_rate() <= 0)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Sampling rate is invalid for {} - skipping", name);
                    continue;
                }
                if (!packet.has_data_type())
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Packet for {} does not have a data type - skipping",
                         name);
                    continue;
                }
                if (!packet.has_number_of_samples())
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                       "Packet for {} does not have a samples count - skipping",
                       name);
                    continue;
                }
                // Is the packet too old?
                auto endTime
                    = Utilities::getEndTime<std::chrono::nanoseconds> (packet);
                auto now = Utilities::getNow<std::chrono::nanoseconds> ();
                if (endTime < now - maximumLatency)
                {
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "Skipping {} because it is too old",
                                        name);
                    continue;
                }
                // Add the packet to the circular buffer map
                try
                {
 
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                       "Failed to add {} to circular buffer map because {}",
                       name, std::string {e.what()}); 
                    continue;
                }
            }
            else
            {
                std::this_thread::sleep_for(timeOut);
            }
        }
    }

    /// @brief Periodically prints the status to a summary log.
    void printSummary()
    {
        /// Summary 
        if (mOptions.printSummaryInterval.count() <= 0){return;}
        const auto now =
            std::chrono::duration_cast<std::chrono::seconds>
            ((std::chrono::high_resolution_clock::now()).time_since_epoch());
        if (now < mLastReport + mOptions.printSummaryInterval){return;}
        mLastReport = now;
    }

    /// @brief Check futures for exceptions.
    /// @result True indicates everything appears to be okay in threads.
    [[nodiscard]] 
    bool areFuturesOkay(const std::chrono::milliseconds &waitForFuture)
    {
        bool isOkay{true};
        for (auto &futuresPair : mFuturesMap)
        {
            try
            {
                auto status = futuresPair.second.wait_for(waitForFuture);
                if (status == std::future_status::ready)
                {
                    futuresPair.second.get();
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_CRITICAL(mLogger,
                                       "Fatal error in {}: {}",
                                       futuresPair.first,
                                       std::string {e.what()});
                isOkay = false;
            }
        }
        return isOkay;
    }

    /// @brief Handles main thread activities.
    void handleMainThread()
    {
        SPDLOG_LOGGER_INFO(mLogger, "Main thread entering waiting loop");
        catchSignals();
        while (!mStopRequested)
        {
            if (mInterrupted)
            {
                SPDLOG_LOGGER_INFO(mLogger,
                                  "SIGINT/SIGTERM signal received!");
                mStopRequested = true;
                break;
            }
            constexpr std::chrono::milliseconds waitForFuture {5};
            if (!areFuturesOkay(waitForFuture))
            {
                SPDLOG_LOGGER_CRITICAL(mLogger,
                   "Futures exception caught; terminating app");
                mStopRequested = true;
                break;
            }
            printSummary();
            std::unique_lock<std::mutex> lock(mStopMutex);
            constexpr std::chrono::milliseconds pause{100};
            mStopCondition.wait_for(lock, pause,
                                    [this]
                                    {
                                        return mStopRequested;
                                    });
        }
        if (mStopRequested)
        {
            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Stop request received.  Terminating...");
            stop();
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
        }
    }

    /// @brief Defines the signals we'll react to.
    void catchSignals()
    {   
        std::signal(SIGINT,  Process::signalHandler);
        std::signal(SIGTERM, Process::signalHandler);
    }   

    static void signalHandler(const int signal)
    {   
        mSignalStatus = signal;
        mInterrupted.store(true);
    }   
//public:
    ::ProgramOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::unique_ptr<Service> mService{nullptr}; 
    std::unique_ptr<Subscriber> mDataPacketSubscriber{nullptr};
    mutable std::mutex mStopMutex;
    std::condition_variable mStopCondition;
    tbb::concurrent_bounded_queue
    <
        UDataPacketServiceAPI::V1::Packet
    > mImportQueue;
    std::function<void(UDataPacketServiceAPI::V1::Packet &&)>
        mAddPacketCallback
    {   
        std::bind(&Process::addPacket, this,
                  std::placeholders::_1)
    };  
    std::chrono::seconds mLastReport
    {
        std::chrono::duration_cast<std::chrono::seconds>
        ((std::chrono::high_resolution_clock::now()).time_since_epoch())
    };
    std::map<std::string, std::future<void>> mFuturesMap;
    int64_t mMaximumImportQueueSize{4096};
    bool mStopRequested{false};
    bool mIsRunning{false};
    std::atomic<bool> mKeepRunning{true};
};

int main(int argc, char *argv[])
{
    // Get this out of the way
    initializeMetricsSingleton();

    // Parse the command line arguments
    std::filesystem::path iniFile;
    try
    {
        auto [iniFileName, isHelp] = ::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        if (iniFileName.empty())
        {
            throw std::runtime_error("No initialization file specified");
        }
        iniFile = iniFileName;

    }
    catch (const std::exception &e)
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        auto logger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_ERROR(logger,
                            "Failed to read command line options because {}",
                            std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Read the program options from the ini file
    ::ProgramOptions programOptions;
    try
    {
        programOptions = ::parseIniFile(iniFile);
    }
    catch (const std::exception &e)
    {
        //NOLINTNEXTLINE(misc-include-cleaner
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read program options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }
    if (std::getenv("OTEL_SERVICE_NAME") == nullptr)
    {
        constexpr int overwrite{1};
        setenv("OTEL_SERVICE_NAME",
               programOptions.applicationName.c_str(),
               overwrite);
    }

    // Create the logger
    std::shared_ptr<spdlog::logger> logger{nullptr};
    try 
    {
        logger = UDataPacketCacheService::Logger::initialize(programOptions);
    }
    catch (const std::exception &e)
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to initialize logger because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    std::unique_ptr<::Process> process{nullptr};
    try
    {
        SPDLOG_LOGGER_INFO(logger, "Initializing main thread");
        process
            = std::make_unique<::Process> (std::move(programOptions), logger);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to initialize main process because {}",
                               std::string {e.what()});
        UDataPacketCacheService::Metrics::cleanup();
        UDataPacketCacheService::Logger::cleanup();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
