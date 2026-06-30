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
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include "uDataPacketCacheService/streamDequeMap.hpp"
#include "uDataPacketCacheService/streamDequeMapOptions.hpp"
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
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif

        mMaximumImportQueueSize = mOptions.maximumImportQueueSize;
        mImportQueue.set_capacity(mMaximumImportQueueSize);

StreamDequeMapOptions dmopt;
        mStreamDequeMap = std::make_unique<StreamDequeMap> (dmopt, mLogger);
 
        mDataPacketSubscriber
            = std::make_unique<Subscriber> (
                mOptions.dataPacketSubscriberOptions,
                mAddPacketCallback,
                mLogger);

        mService
            = std::make_unique<Service> (
                mOptions.serviceOptions,
                mStreamDequeMap,
                mLogger);
    }

    /// @brief Destructor
    ~Process()
    {
        stop();
    }

    void start()
    {
        mKeepRunning.store(true);

        // Make sure packet processor is ready to work
        auto packetProcessorFuture = std::async(&Process::processPackets, this);
        mFuturesMap.insert_or_assign("PacketProcessor", std::move(packetProcessorFuture));

        // Start getting packets
        auto packetSubscriberFuture = mDataPacketSubscriber->start();
        mFuturesMap.insert_or_assign("DataPacketSubscriber", std::move(packetSubscriberFuture));

        // Won't get packets so fast we need to immediately clean
        auto cleanDequesFuture = std::async(&Process::cleanDeques, this);
        mFuturesMap.insert_or_assign("CleanDeques", std::move(cleanDequesFuture));

        // Finally, open this for business
        // TODO add service

        mIsRunning = true;
        handleMainThread();
    }

    /// @brief Stops processes
    void stop()
    {
        // Help my cleaning thread out
        mTerminateRequested = true;
        mTerminateCondition.notify_all();

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

        for (auto &futurePair : mFuturesMap) 
        {
            if (futurePair.second.valid()){futurePair.second.get();}
        }
    }

    /// Allows import thread to add packets
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        // Make sure there's space in the queue
        int nPopped{0};
        if (static_cast<size_t> (mImportQueue.size()) >= 
            mMaximumImportQueueSize)
        {
            nPopped = nPopped + 1;
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
        if (nPopped > 0)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Overfull import queue - popped {} packets",
                               nPopped);
        }
        // Add it
        if (!mImportQueue.try_push(std::move(packet)))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to enqueue packet");
        }
    }

    /// Clean packets
    void cleanDeques()
    {
        constexpr std::chrono::seconds cleanupInterval{30};
        int nConsecutiveErrors{0};
        while (mKeepRunning.load())
        {
            try
            {
                mStreamDequeMap->removeExpiredPackets();
                nConsecutiveErrors = 0;
            }
            catch (const std::exception &e)
            {
                nConsecutiveErrors = nConsecutiveErrors + 1;
                SPDLOG_LOGGER_WARN(mLogger,
                    "Failed to remove expired packets from deques because {}",
                    std::string {e.what()});
            }
            if (nConsecutiveErrors > 5)
            {
                throw std::runtime_error(
                    "Too many consective errosr cleaning deques");
            }
            // Wait
            std::unique_lock<std::mutex> lock(mStopMutex);
            mTerminateCondition.wait_for(lock, cleanupInterval,
                                         [this]
                                         {   
                                            return mTerminateRequested;
                                         }); 
        }
    } 

    /// Propagate packets to the backend
    void processPackets()
    {
        SPDLOG_LOGGER_INFO(mLogger, "Packet processing thread started");
        auto &metrics
            = UDataPacketCacheService::MetricsSingleton::getInstance();
        auto maximumLatency
            = std::chrono::nanoseconds{mOptions.maximumPacketLatency};
        const bool checkLatency = maximumLatency.count() > 0 ? true : false;
        auto maximumFutureTime
            = std::chrono::nanoseconds{mOptions.maximumPacketFutureTime};
        const bool checkFuture = maximumFutureTime.count() >= 0 ? true : false;
        constexpr std::chrono::milliseconds timeOut{10};
        while (mKeepRunning.load())
        {
            UDataPacketServiceAPI::V1::Packet packet;
            auto gotPacket = mImportQueue.try_pop(packet);
            if (gotPacket)
            {
                // Check packet
                metrics.incrementPacketsReceivedCounter();
                std::string reason;
                if (!Utilities::isValid(packet, reason))
                {
                    metrics.incrementInvalidPacketsReceivedCounter();
                    SPDLOG_LOGGER_DEBUG(mLogger, "Skipping packet because {}",
                                        reason);
                }

                // Is the packet too old?
                if (checkLatency || checkFuture)
                {
                    auto now = Utilities::getNow<std::chrono::nanoseconds> (); 
                    if (checkLatency)
                    {
                        metrics.incrementInvalidPacketsReceivedCounter();
                        auto startTime
                            = Utilities::getStartTime<std::chrono::nanoseconds>
                              (packet);
                        if (startTime < now - maximumLatency)
                        {
                            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Skipping {} because it is has expired data",
                                 name);
                            continue;
                        }
                    }
                    if (checkLatency)
                    {
                        metrics.incrementInvalidPacketsReceivedCounter();
                        auto endTime
                            = Utilities::getStartTime<std::chrono::nanoseconds>
                              (packet);
                        if (endTime > now + maximumFutureTime)
                        {
                            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Skipping {} because it has data from future",
                                 name);
                            continue;
                        }
                    }
                }
                // Add the packet to the collection of stream deques
                try
                {
                    mStreamDequeMap->addPacket(std::move(packet)); 
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                       "Failed to add packet to stream deque map because {}",
                       std::string {e.what()}); 
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
        while (!mStopProcessRequested)
        {
            if (mInterrupted)
            {
                SPDLOG_LOGGER_INFO(mLogger,
                                  "SIGINT/SIGTERM signal received!");
                mStopProcessRequested = true;
                break;
            }
            constexpr std::chrono::milliseconds waitForFuture {5};
            if (!areFuturesOkay(waitForFuture))
            {
                SPDLOG_LOGGER_CRITICAL(mLogger,
                   "Futures exception caught; terminating app");
                mStopProcessRequested = true;
                break;
            }
            printSummary();
            std::unique_lock<std::mutex> lock(mStopMutex);
            constexpr std::chrono::milliseconds pause{100};
            mStopProcessCondition.wait_for(lock, pause,
                                           [this]
                                           {
                                               return mStopProcessRequested;
                                           });
        }
        if (mStopProcessRequested)
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
    std::shared_ptr<StreamDequeMap> mStreamDequeMap{nullptr};
    mutable std::mutex mStopMutex;
    std::condition_variable mStopProcessCondition;
    std::condition_variable mTerminateCondition;
    tbb::concurrent_bounded_queue
    <
        UDataPacketServiceAPI::V1::Packet
    > mImportQueue;
    UDataPacketCacheService::MetricsSingleton &mMetrics
    {
        UDataPacketCacheService::MetricsSingleton::getInstance()
    };
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
    bool mStopProcessRequested{false};
    bool mTerminateRequested{false};
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

    try
    {
        process->start();
        UDataPacketCacheService::Metrics::cleanup();
        UDataPacketCacheService::Logger::cleanup();
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Application failed because {}",
                               std::string {e.what()});
        UDataPacketCacheService::Metrics::cleanup();
        UDataPacketCacheService::Logger::cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
