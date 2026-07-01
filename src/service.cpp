#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/time.h> //NOLINT
#include <grpcpp/impl/channel_argument_option.h>
#include <grpc/impl/compression_types.h>
#include <uDataPacketCacheServiceAPI/v1/service.grpc.pb.h>
#include <uDataPacketCacheServiceAPI/v1/available_streams_request.pb.h>
#include <uDataPacketCacheServiceAPI/v1/available_streams_response.pb.h>
#include <uDataPacketCacheServiceAPI/v1/data_request.pb.h>
#include <uDataPacketCacheServiceAPI/v1/data_response.pb.h>
#include <uDataPacketCacheServiceAPI/v1/time_series.pb.h>
#include "uDataPacketCacheService/service.hpp"
#include "uDataPacketCacheService/serviceOptions.hpp"
#include "uDataPacketCacheService/grpcServerOptions.hpp"
#include "uDataPacketCacheService/streamDequeMap.hpp"
#include "uDataPacketCacheService/metricsSingleton.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

namespace
{

[[nodiscard]] 
bool validateClient(const grpc::CallbackServerContext *context,
                     const std::string &accessToken)
{
    if (accessToken.empty()) { return true; }
    for (const auto &item : context->client_metadata())
    {
        if (item.first == "x-custom-auth-token")
        {
            if (item.second == accessToken) { return true; }
        }
    }
    return false;
}

struct Request
{
    explicit Request(const UDataPacketCacheServiceAPI::V1::StreamRequest &request)
    {
        identifier = request.stream_identifier();
        auto requestStartTime
            = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                request.start_time());
        auto requestEndTime
            = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                request.end_time());
        startAndEndTime
            = std::make_pair(
                 std::chrono::nanoseconds {requestStartTime},
                 std::chrono::nanoseconds {requestEndTime}
             );
        identifierString = Utilities::toString(identifier);
        duplicateRequestIndex =-1;
        globalRequestIndex =-1;
    }
    UDataPacketCacheServiceAPI::V1::StreamIdentifier identifier;
    std::pair<std::chrono::nanoseconds, 
              std::chrono::nanoseconds> startAndEndTime;
    std::string identifierString;
    int duplicateRequestIndex{-1};
    int globalRequestIndex{-1};
};

void setDataPackets(
     std::vector<UDataPacketCacheServiceAPI::V1::Packet> &dataPackets,
     UDataPacketCacheServiceAPI::V1::TimeSeries *timeSeries)
{
     if (!dataPackets.empty())
     {
         timeSeries->mutable_packets()->Reserve(
             static_cast<int> (dataPackets.size()));
         for (auto &dataPacket : dataPackets)
         {
             timeSeries->mutable_packets()->Add(std::move(dataPacket));
         }
     }
     else
     {
         timeSeries->Clear();
     }
}


bool operator==(const Request &lhs, const Request &rhs)
{
     // No point trying to match to an un-matched request
     if (lhs.duplicateRequestIndex ==-1 &&
         rhs.duplicateRequestIndex ==-1)
     {
         return false;
     }         
     if (lhs.startAndEndTime.first != rhs.startAndEndTime.first)
     {
         return false;
     }
     if (lhs.startAndEndTime.second != rhs.startAndEndTime.second)
     {
         return false;
     }
     if (lhs.identifierString != rhs.identifierString)
     {
         return false;
     }
     return true;
}

}

class Service::ServiceImpl : 
    public UDataPacketCacheServiceAPI::V1::DataPacketCacheService::CallbackService
{
public:
    // Constructor
    ServiceImpl(const ServiceOptions &options,
                std::shared_ptr<StreamDequeMap> streamDequeMap,
                std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mStreamDequeMap(std::move(streamDequeMap)),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasGRPCOptions())
        {
            throw std::invalid_argument("gRPC options not set for server");
        }
        if (mStreamDequeMap == nullptr)
        {
            throw std::invalid_argument("Stream deque map is null");
        }
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            auto classId
                = std::to_string (reinterpret_cast<std::uintptr_t> (this));
            mLogger = spdlog::stdout_color_mt("ServiceConsole-" + classId);
            // NOLINTEND(misc-include-cleaner)
        }
        mMaximumNumberOfClients
            = mOptions.getMaximumNumberOfConcurrentStreams();
    }

    // Available streams - more of a debugging tool than anything useful
    grpc::ServerUnaryReactor
       *GetAvailableStreams(
            grpc::CallbackServerContext *context,
            const UDataPacketCacheServiceAPI::V1::AvailableStreamsRequest *request,
            UDataPacketCacheServiceAPI::V1::AvailableStreamsResponse *response) override
    {
        class Reactor : public grpc::ServerUnaryReactor
        {
        public:
            Reactor(grpc::CallbackServerContext *context,
                    const UDataPacketCacheServiceAPI::V1::AvailableStreamsRequest &request,
                    UDataPacketCacheServiceAPI::V1::AvailableStreamsResponse *response,
                    const bool isSecured,
                    const GRPCServerOptions &grpcOptions,
                    StreamDequeMap &streamDequeMap,
                    std::shared_ptr<spdlog::logger> logger) :
                mLogger(std::move(logger))
            {
                auto &metrics = MetricsSingleton::getInstance();
                metrics.incrementNumberOfClients();
                // Validate the client?
                if (isSecured)
                {
                    auto accessToken = grpcOptions.getAccessToken();
                    if (accessToken)
                    {
                        if (!::validateClient(context, *accessToken))
                        {
                            SPDLOG_LOGGER_WARN(mLogger,
                                              "Unauthorized client {} rejected",
                                               context->peer());
                            metrics.incrementInvalidAccessCounter();
                            Finish({grpc::StatusCode::UNAUTHENTICATED,
                                    "Invalid access token"});
                            return;
                        }
                    }
                }
                // Copy the request identifier
                std::string requestIdentifier{context->peer()};
                if (request.has_identifier())
                {
                    requestIdentifier = requestIdentifier
                                      + " ("
                                      + request.identifier()
                                      + ")";
                    *response->mutable_identifier() = request.identifier();
                }
#ifndef NDEBUG
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "Getting available streams for {}",
                                    requestIdentifier);
#endif
                // Do it
                try
                {
                    auto streams = streamDequeMap.getAvailableStreams();
                    for (auto &stream : streams)
                    {
                        response->mutable_stream_identifiers()->Add(
                            std::move(stream));
                    }
                }
                catch (const std::exception &e)
                {
                    metrics.incrementServerErrorCounter();
                    SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to get available streams because {}",
                                    std::string{e.what()});
                    Finish({grpc::StatusCode::INTERNAL,
                            "Server error - try using a different endpoint"});
                    return;
                }
                mSuccess = true;
                Finish(grpc::Status::OK);
#ifndef NDEBUG
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "Successfully found streams for {}",
                                    requestIdentifier);
#endif
            }
        private:
            void OnDone() override
            {
#ifndef NDEBUG
                if (mLogger)
                {
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "GetAvailableStreams RPC completed");
                }
#endif
                auto &metrics = MetricsSingleton::getInstance();
                metrics.decrementNumberOfClients();
                if (mSuccess)
                {
                    auto endTime
                        = Utilities::getNow<std::chrono::nanoseconds> ();
                    auto elapsedTime
                        = static_cast<double>
                          (endTime.count() - mRPCStartTime.count())*1.e-9;
                }
                delete this;
            }
            void OnCancel() override
            {
#ifndef NDEBUG
                if (mLogger)
                {
                   SPDLOG_LOGGER_DEBUG(mLogger,
                                       "GetAvailableStreams RPC canceled");
                }
#endif
            }
//private:
            std::shared_ptr<spdlog::logger> mLogger{nullptr};
            std::chrono::nanoseconds mRPCStartTime
            {
                Utilities::getNow<std::chrono::nanoseconds> ()
            };
            bool mSuccess{false};
        };
        return new Reactor(context,
                           *request,
                           response,
                           mSecured,
                           mGRPCOptions,
                           *mStreamDequeMap,
                           mLogger);
    }
    // Time series request
    grpc::ServerUnaryReactor
       *GetTimeSeries(grpc::CallbackServerContext *context,
                      const UDataPacketCacheServiceAPI::V1::DataRequest *request,
                      UDataPacketCacheServiceAPI::V1::DataResponse *response) override
    {
        class Reactor : public grpc::ServerUnaryReactor
        {
        public:
            Reactor(grpc::CallbackServerContext *context,
                    const UDataPacketCacheServiceAPI::V1::DataRequest &request,
                    UDataPacketCacheServiceAPI::V1::DataResponse *response,
                    const bool isSecured,
                    const GRPCServerOptions &grpcOptions,
                    StreamDequeMap &streamDequeMap,
                    std::shared_ptr<spdlog::logger> logger) :
                mLogger(std::move(logger))
            {
                auto &metrics = MetricsSingleton::getInstance();
                metrics.incrementNumberOfClients();
                // Validate the client?
                if (isSecured)
                {
                    auto accessToken = grpcOptions.getAccessToken();
                    if (accessToken)
                    {
                        if (!::validateClient(context, *accessToken))
                        {
                            SPDLOG_LOGGER_WARN(mLogger,
                                              "Unauthorized client {} rejected",
                                               context->peer());
                            metrics.incrementInvalidAccessCounter();
                            Finish({grpc::StatusCode::UNAUTHENTICATED,
                                    "Invalid access token"});
                            return;
                        }
                    }
                }
                // Copy the request identifier
                std::string requestIdentifier{context->peer()};
                if (request.has_identifier())
                {
                    requestIdentifier = requestIdentifier
                                      + " ("
                                      + request.identifier()
                                      + ")";
                    *response->mutable_identifier() = request.identifier();
                }
                SPDLOG_LOGGER_INFO(mLogger,
                                   "Getting time series for {}",
                                   requestIdentifier);
                // Get the requests
                const auto &streamRequests = request.stream_requests();
                // Easy case - nothing desired, nothing returned
                if (streamRequests.empty())
                {
                    metrics.incrementInvalidRequestCounter();
                    Finish({grpc::StatusCode::INVALID_ARGUMENT,
                            "No streams specified in request"});
                    return;
                }
                // Don't need some unnecessary checking for just one request
                if (streamRequests.size() == 1)
                {
                    std::string reason;
                    if (!Utilities::isValid(streamRequests[0], reason))
                    {
                        metrics.incrementInvalidRequestCounter();
                        Finish({grpc::StatusCode::INVALID_ARGUMENT, reason});
                        return;
                    }
                    ::Request request{streamRequests[0]};
                    try
                    {
                        auto dataPackets
                            = streamDequeMap.getPackets(request.identifier,
                                                        request.startAndEndTime);
                        UDataPacketCacheServiceAPI::V1::TimeSeries timeSeries;
                        *timeSeries.mutable_stream_identifier()
                            = std::move(request.identifier);
                        ::setDataPackets(dataPackets, &timeSeries);
                        //timeSeries.mutable_packets()->Assign(dataPackets.begin(), dataPackets.end());
                        response->mutable_time_series()->Add(
                            std::move(timeSeries));
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_ERROR(mLogger,
                                            "Failed to get packets because {}",
                                            std::string {e.what()});
                        Finish({grpc::StatusCode::INTERNAL,
                            "Server error - try using a different endpoint"});
                        metrics.incrementServerErrorCounter();
                        return;
                    } 
                    auto endTime
                        = Utilities::getNow<std::chrono::nanoseconds> ();
                    metrics.incrementSuccessfulRPCCounter();
                    Finish(grpc::Status::OK);
                    return;
                }

                // This is a bear - we have to validate requests and deduplicate
                std::vector<::Request> requests;
                requests.reserve(streamRequests.size());

                // Build up the requests (noting the duplicates)
                bool hasDuplicates{false};
                for (int ir = 0; 
                     ir < static_cast<int> (streamRequests.size()); ++ir)
                {
                    std::string reason;
                    if (!Utilities::isValid(streamRequests[ir], reason))
                    {
                        reason.append(" for stream request ");
                        reason.append(std::to_string(ir));
                        Finish({grpc::StatusCode::INVALID_ARGUMENT, reason});
                        metrics.incrementInvalidRequestCounter();
                        return;
                    }
                    // See if this matches a previous request
                    Request request{streamRequests[ir]};
                    for (int jr = 0;
                         jr < static_cast<int> (requests.size()); ++jr)
                    {
                        if (request == requests[jr])
                        {
                            hasDuplicates = true;
                            request.duplicateRequestIndex = jr;
                            request.globalRequestIndex = ir;
                            break;
                        }
                    }
                }
                std::vector<UDataPacketCacheServiceAPI::V1::TimeSeries> timeSeriesFromQuery;
                timeSeriesFromQuery.resize(streamRequests.size());
                // N.B. this could be run in parallel
                for (int ir = 0;  
                     ir < static_cast<int> (requests.size()); ++ir)
                {
                    std::vector<UDataPacketCacheServiceAPI::V1::Packet> dataPackets;
                    if (requests[ir].duplicateRequestIndex ==-1)
                    {
                        try
                        {
                            dataPackets
                                = streamDequeMap.getPackets(
                                    requests[ir].identifier,
                                    requests[ir].startAndEndTime);
                            UDataPacketCacheServiceAPI::V1::TimeSeries
                                timeSeries;
                            *timeSeries.mutable_stream_identifier()
                                = std::move(requests[ir].identifier);
                            ::setDataPackets(dataPackets, &timeSeries);
                            timeSeriesFromQuery[requests[ir].globalRequestIndex]
                                = std::move(timeSeries); 
                        }
                        catch (const std::exception &e)
                        {
                            SPDLOG_LOGGER_ERROR(mLogger,
                                            "Failed to get packets because {}",
                                            std::string {e.what()});
                            Finish({grpc::StatusCode::INTERNAL,
                            "Server error - try using a different endpoint"});
                            metrics.incrementServerErrorCounter();
                            return;
                        }
                    }
                }
                // Copy the duplicate requests 
                if (hasDuplicates)
                {
                    for (const auto &request : requests)
                    {
                        if (request.duplicateRequestIndex !=-1)
                        {
                            timeSeriesFromQuery[request.duplicateRequestIndex]
                            = timeSeriesFromQuery[request.globalRequestIndex];
                        }
                    } 
                }
                // And finally set the time series on the responses
                for (auto &timeSeries : timeSeriesFromQuery)
                {
                    response->mutable_time_series()->Add(
                        std::move(timeSeries));
                }
                mSuccess = true;
                Finish(grpc::Status::OK);
            }
        private:
            void OnDone() override
            {
#ifndef NDEBUG
                if (mLogger)
                {
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "GetTimeSeries RPC completed");
                }
#endif
                auto &metrics = MetricsSingleton::getInstance();
                metrics.decrementNumberOfClients();
                if (mSuccess)
                {
                    auto endTime
                        = Utilities::getNow<std::chrono::nanoseconds> ();
                    auto elapsedTime
                        = static_cast<double>
                          (endTime.count() - mRPCStartTime.count())*1.e-9;
                }
                delete this;
            }
            void OnCancel() override
            {
#ifndef NDEBUG
                if (mLogger)
                {
                   SPDLOG_LOGGER_DEBUG(mLogger,
                                       "GetTimeSeries RPC canceled");
                }
#endif
            }
//private:
            std::shared_ptr<spdlog::logger> mLogger{nullptr};
            std::chrono::nanoseconds mRPCStartTime
            {
                Utilities::getNow<std::chrono::nanoseconds> ()
            };
            bool mSuccess{false};
        }; 
        auto compression = mOptions.getCompressionAlgorithm();
        if (compression ==
            ServiceOptions::CompressionAlgorithm::Deflate)
        {
            context->set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
        }
        else if (compression ==
                 ServiceOptions::CompressionAlgorithm::GZIP)
        {
            context->set_compression_algorithm(GRPC_COMPRESS_GZIP);
        }
        return new Reactor(context,
                           *request,
                           response,
                           mSecured,
                           mGRPCOptions,
                           *mStreamDequeMap,
                           mLogger);
    }

    void start()
    {
        mKeepRunning.store(true);

        mGRPCOptions = mOptions.getGRPCOptions();
        const auto address = mGRPCOptions.getHost() + ":"
                           + std::to_string(mGRPCOptions.getPort());
        grpc::ServerBuilder builder;
        builder.SetMaxSendMessageSize(
            mOptions.getMaximumRequestMessageSizeInBytes());
        builder.SetMaxSendMessageSize(
            mOptions.getMaximumRequestMessageSizeInBytes());
        builder.SetOption(grpc::MakeChannelArgumentOption(
               "GRPC_ARG_MAX_CONNECTION_AGE_MS",
               static_cast<int>
                  (mOptions.getMaximumConnectionAge().count())));
        builder.SetOption(grpc::MakeChannelArgumentOption(
               "GRPC_ARG_MAX_CONNECTION_AGE_GRACE_MS",
               static_cast<int>
                   (mOptions.getMaximumConnectionAgeGracePeriod().count())));
        builder.SetOption(grpc::MakeChannelArgumentOption(
               "GRPC_ARG_MAX_CONCURRENT_STREAMS",
                mOptions.getMaximumNumberOfConcurrentStreams()));

        if (mGRPCOptions.getServerKey() == std::nullopt ||
            mGRPCOptions.getServerCertificate() == std::nullopt)
        {        
            SPDLOG_LOGGER_INFO(mLogger,
                               "Initiating non-secured subscribe service");
            builder.AddListeningPort(address,
                                     grpc::InsecureServerCredentials());
            mSecured = false;
        }
        else
        {
            auto serverKey = mGRPCOptions.getServerKey();
            auto serverCertificate = mGRPCOptions.getServerCertificate();
#ifndef NDEBUG
            assert(serverKey != std::nullopt);
            assert(serverCertificate != std::nullopt);
#endif
            SPDLOG_LOGGER_INFO(mLogger, "Initiating secured subscribe service");
            const grpc::SslServerCredentialsOptions::PemKeyCertPair keyCertPair
            {
                *serverKey,
                *serverCertificate
            };
            grpc::SslServerCredentialsOptions sslOptions;
            sslOptions.pem_key_cert_pairs.emplace_back(keyCertPair);
            builder.AddListeningPort(address,
                                     grpc::SslServerCredentials(sslOptions));
            mSecured = true;
        }
        SPDLOG_LOGGER_INFO(mLogger, "Building server");
        builder.RegisterService(this);
        SPDLOG_LOGGER_INFO(mLogger,
                           "DataPacketCacheService listening at {}", address);
        mServer = builder.BuildAndStart();
        mServer->Wait();
        mStarted = true;
    }

    void stop()
    {
        mKeepRunning.store(false);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        if (mServer)
        {
            if (mStarted)
            {
                SPDLOG_LOGGER_INFO(mLogger, "Shutting down service");
            }
            constexpr int64_t timeOutSeconds{1};
            constexpr int64_t timeOutNanoSeconds{0};
            const gpr_timespec deadline // NOLINT
            {
                timeOutSeconds,
                timeOutNanoSeconds,
                GPR_TIMESPAN // NOLINT
            };
            mServer->Shutdown(deadline);
            if (mStarted)
            {
                SPDLOG_LOGGER_INFO(mLogger, "Service shut down");
            }
            mServer = nullptr;
        }
        mStarted = false;
    }

    ~ServiceImpl() override
    {
        stop();
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
 
    ServiceImpl() = delete;
//private: 
    ServiceOptions mOptions; 
    std::shared_ptr<StreamDequeMap> mStreamDequeMap{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::unique_ptr<grpc::Server> mServer{nullptr};
    GRPCServerOptions mGRPCOptions;
    std::atomic<int> mNumberOfClients{0};
    std::atomic<bool> mKeepRunning{true};
    int mMaximumNumberOfClients{64};
    bool mSecured{false};
    bool mStarted{false};
};

/// Constructor
Service::Service(const ServiceOptions &options,
                 std::shared_ptr<StreamDequeMap> streamDequeMap,
                 std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<ServiceImpl> (options,
                                         std::move(streamDequeMap),
                                         std::move(logger)))
{
}

/// Start the service
std::future<void> Service::start()
{
    return std::async(&ServiceImpl::start, &*pImpl);
}

/// Stop the service
void Service::stop()
{
    pImpl->stop();
}


/// Destructor
Service::~Service() = default;
