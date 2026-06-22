#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
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
#include "uDataPacketCacheService/service.hpp"
#include "uDataPacketCacheService/metricsSingleton.hpp"
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
            mIsRunning = false;
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
    //std::unique_ptr<Service> mService{nullptr}; 
    //std::unique_ptr<Subscriber> mDataPacketClient{nullptr};
    mutable std::mutex mStopMutex;
    std::condition_variable mStopCondition;
    std::chrono::seconds mLastReport
    {
        std::chrono::duration_cast<std::chrono::seconds>
        ((std::chrono::high_resolution_clock::now()).time_since_epoch())
    };
    std::map<std::string, std::future<void>> mFuturesMap;
    bool mStopRequested{false};
    bool mIsRunning{false};
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
