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
#include <spdlog/sinks/stdout_color_sinks.h> // NOLINT(misc-include-cleaner)
#include <opentelemetry/metrics/provider.h>
//#include <opentelemetry/metrics/meter.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include "uDataPacketCacheService/streamDequeMap.hpp"
#include "uDataPacketCacheService/service.hpp"
#include "uDataPacketCacheService/subscriber.hpp"
#include "uDataPacketCacheService/metricsSingleton.hpp"
#include "uDataPacketCacheService/utilities.hpp"
#include "uDataPacketCacheService/version.hpp"
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

        mStreamDequeMap
            = std::make_unique<StreamDequeMap>
              (mOptions.streamDequeMapOptions, mLogger);
 
        mDataPacketSubscriber
            = std::make_unique<Subscriber> (
                mOptions.dataPacketSubscriberOptions,
                mAddPacketCallback,
                mLogger);

        mService
            = std::make_unique<Service> (
                mOptions.serviceOptions,
                mStreamDequeMap,
                mLogger,
                mRecordDurationForMetrics);

        // Metrics
        if (mOptions.exportMetrics)
        {
            auto &metrics = MetricsSingleton::getInstance();
            metrics.setMaximumNumberOfClients(
                mOptions.serviceOptions.getMaximumNumberOfConcurrentStreams());
            // Need a provider from which to get a meter.  This is initialized
            // once and should last the duration of the application.
            auto provider
                = opentelemetry::metrics::Provider::GetMeterProvider();

            // Meter will be bound to application (library, module, class, etc.)
            // so as to identify who is genreating these metrics.
            auto meter = provider->GetMeter(mOptions.applicationName, "1.2.0");

            // Packets received
            receivedPacketsCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.client.packets.received.",
                  "Number of seismic data packets received by the import client",
                  "{packets}");
            receivedPacketsCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfPacketsReceived,
                nullptr);

            invalidPacketsCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.client.packets.invalid",
                  "Number of seismic data packets received by the import client that were invalid or malformed",
                  "{packets}");
            receivedPacketsCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfInvalidPacketsReceived,
                nullptr);

            importOverflowPacketsCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.client.packets.overflow",
                  "Number of seismic data packets that were skipped because of an internal buffer overflow",
                  "{packets}");
            importOverflowPacketsCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfOverflowPackets,
                nullptr);

            invalidAccessCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.service.access_denied",
                  "Number of times clients have been denied access to an RPC - these are 300 errors");
            invalidAccessCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfInvalidAccesses,
                nullptr);

            invalidRequestCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.service.invalid_request",
                  "Number of times clients have submitted invalid queries to an RPC - these are 400 errors");
            invalidRequestCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfInvalidRequests,
                nullptr);

            serverErrorCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.service.server_error",
                  "Number of times an internal error was detected in an RPC - these are 500 errors");

            serverErrorCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfServerErrors,
                nullptr);

            successfulRPCCounter
                = meter->CreateInt64ObservableCounter(
                  "seismic_data.waveform_storage.packet_cache.service.success",
                  "Number of times an RPC was successfully called - these are 200 response codes");
            successfulRPCCounter->AddCallback(
                UDataPacketCacheService::Metrics::observeNumberOfSuccesses,
                nullptr);

            auto histgoramMeter = provider->GetMeter("rpc_duration", "1.2.0");
            rpcServiceHistogram
                = histgoramMeter->CreateDoubleHistogram(
                    "seismic_data.waveform_storage.service.duration",
                    "Time required to process a waveform request as a histogram.",
                    "{s}");
        }
    }

    /// @brief Destructor
    ~Process()
    {
        stop();
    }

    void start()
    {
        mKeepRunning.store(true);

        // Lightweight to launch thread - let it rip through and find nothing
        auto cleanDequesFuture = std::async(&Process::cleanDeques, this);
        mFuturesMap.insert_or_assign("CleanDeques",
                                     std::move(cleanDequesFuture));

        // Make sure packet processor is ready to work
        auto packetProcessorFuture = std::async(&Process::processPackets, this);
        mFuturesMap.insert_or_assign("PacketProcessor",
                                     std::move(packetProcessorFuture));

        // Start getting packets
        auto packetSubscriberFuture = mDataPacketSubscriber->start();
        mFuturesMap.insert_or_assign("DataPacketSubscriber",
                                     std::move(packetSubscriberFuture));

        // Finally, open this for business
        auto serviceFuture = mService->start();
        mFuturesMap.insert_or_assign("gRPCService", std::move(serviceFuture));

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
            mIsRunning = false;
            // Stop the import first
            if (mDataPacketSubscriber != nullptr)
            {
                mDataPacketSubscriber->stop();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
            // Stop propagating the packets (that aren't coming)
            mKeepRunning.store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
            // Now boot the clients (hopefully they got everything by now)
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
                    "Too many consective errors cleaning deques");
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
                    continue;
                }

                // Is the packet too old?
                if (checkLatency || checkFuture)
                {
                    auto now = Utilities::getNow<std::chrono::nanoseconds> (); 
                    if (checkLatency)
                    {
                        auto startTime
                            = Utilities::getStartTime<std::chrono::nanoseconds>
                              (packet);
                        if (startTime < now - maximumLatency)
                        {
                            metrics.incrementInvalidPacketsReceivedCounter();
                            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Skipping {} because it is has expired data",
                                Utilities::toString(packet.stream_identifier()));
                            continue;
                        }
                    }
                    if (checkLatency)
                    {
                        auto endTime
                            = Utilities::getStartTime<std::chrono::nanoseconds>
                              (packet);
                        if (endTime > now + maximumFutureTime)
                        {
                            metrics.incrementInvalidPacketsReceivedCounter();
                            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Skipping {} because it has data from future",
                                Utilities::toString(packet.stream_identifier()));
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
        auto &metrics = MetricsSingleton::getInstance(); 
        auto nPacketsReceived = metrics.getPacketsReceivedCount();
        auto nInvalidPackets = metrics.getInvalidPacketsReceivedCount();
        auto nOverflow = metrics.getImportOverflowPacketCount();
        auto nRejected = metrics.getInvalidAccessCount();
        auto nSuccessfulRequests = metrics.getSuccessfulRPCCount();
        auto nInvalidRequests = metrics.getInvalidRequestCount();
        auto nServerErrors = metrics.getServerErrorCount();
        auto nTotalRequests = nSuccessfulRequests
                            + nInvalidRequests
                            + nServerErrors;
        //auto nClients = metrics.getNumberOfClients();
        //auto utilization = metrics.getServiceUtilization();
        auto nPacketsReport = nPacketsReceived - mPacketsReceivedLastReport; 
        SPDLOG_LOGGER_INFO(mLogger, "Since last report: Received {} packets ({} invalid, {} lost due to buffer overflow).  Rejected {} accesses.  Processed {} requests ({} succeeded, {} invalid, {} failed).",
           nPacketsReceived - mPacketsReceivedLastReport,
           nInvalidPackets - mInvalidPacketsLastReport, 
           nOverflow - mOverflowLastReport, 
           nRejected - mRejectedLastReport, 
           nTotalRequests - mTotalRequestsLastReport,
           nSuccessfulRequests - mSuccessfulRequestsLastReport,
           nInvalidRequests - mInvalidRequestsLastReport,
           nServerErrors - mServerErrorsLastReport
           //nClients, utilization
           ); 
        mPacketsReceivedLastReport = nPacketsReceived;
        mInvalidPacketsLastReport = nInvalidPackets; 
        mOverflowLastReport = nOverflow;
        mRejectedLastReport = nRejected;
        mTotalRequestsLastReport = nTotalRequests;
        mSuccessfulRequestsLastReport = nSuccessfulRequests;
        mInvalidRequestsLastReport = nInvalidRequests;
        mServerErrorsLastReport = nServerErrors;
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

    void recordDurationForMetrics(const double duration,
                                  const std::string &endPoint)
    {
        if (rpcServiceHistogram != nullptr)
        {
            const std::map<std::string, std::string> histogramKey
            {
                {"endpoint", endPoint}
            };
            rpcServiceHistogram->Record(duration, histogramKey);
        }
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
    std::function<void(double, const std::string &)> mRecordDurationForMetrics
    {
        std::bind(&Process::recordDurationForMetrics, this,
                  std::placeholders::_1,
                  std::placeholders::_2)
    };
    std::chrono::seconds mLastReport
    {
        std::chrono::duration_cast<std::chrono::seconds>
        ((std::chrono::high_resolution_clock::now()).time_since_epoch())
    };
    std::map<std::string, std::future<void>> mFuturesMap;
    int64_t mMaximumImportQueueSize{4096};
    int64_t mPacketsReceivedLastReport{0};
    int64_t mInvalidPacketsLastReport{0};
    int64_t mOverflowLastReport{0};
    int64_t mTotalRequestsLastReport{0};
    int64_t mSuccessfulRequestsLastReport{0};
    int64_t mInvalidRequestsLastReport{0};
    int64_t mRejectedLastReport{0};
    int64_t mServerErrorsLastReport{0};
    bool mStopProcessRequested{false};
    bool mTerminateRequested{false};
    bool mIsRunning{false};
    std::atomic<bool> mKeepRunning{true};
};

int main(int argc, char *argv[])
{
    // Prelim stuff to get out of way
    //NOLINTNEXTLINE(misc-include-cleaner)
    auto consoleLogger = spdlog::stdout_color_st("console");
    SPDLOG_LOGGER_INFO(consoleLogger,
                      "Running version {} of uDataPacketCacheService",
                      UDataPacketCacheService::Version::getVersionWithTag());
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
        SPDLOG_LOGGER_ERROR(consoleLogger,
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

    // Create the real logger
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
