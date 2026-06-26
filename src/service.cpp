#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/time.h> //NOLINT
#include <uDataPacketCacheServiceAPI/v1/service.grpc.pb.h>
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

}

class Service::ServiceImpl : 
    public UDataPacketCacheServiceAPI::V1::DataPacketCacheService::CallbackService
{
public:
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
                auto startTime
                    = Utilities::getNow<std::chrono::nanoseconds> ();
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
                            Finish({grpc::StatusCode::UNAUTHENTICATED,
                                    "Invalid access token"});
                            return;
                        }
                    }
                }
                // Validate the request
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
                                   "Processing waveforms request for {}",
                                   requestIdentifier);

                const auto &streamRequests = request.stream_requests();
                if (streamRequests.empty())
                {
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
                        Finish({grpc::StatusCode::INVALID_ARGUMENT, reason});
                        return;
                    }
                    auto &streamIdentifier = streamRequests[0].stream_identifier();
                    auto requestStartTime
                        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                            streamRequests[0].start_time());
                    auto requestEndTime
                        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                            streamRequests[0].end_time());
                    auto startAndEndTime
                        = std::make_pair(
                             std::chrono::nanoseconds {requestStartTime},
                             std::chrono::nanoseconds {requestEndTime}
                          );
                    auto dataPackets
                        = streamDequeMap.getPackets(streamIdentifier,
                                                    startAndEndTime);
                    auto endTime
                        = Utilities::getNow<std::chrono::nanoseconds> ();
                    Finish(grpc::Status::OK);
                    return;
                }

                // This is a bear - we have to validate requests and deduplicate
                struct Request
                {
                    UDataPacketCacheServiceAPI::V1::StreamIdentifier identifier;
                    std::pair<std::chrono::nanoseconds, 
                              std::chrono::nanoseconds> startAndEndTime;
                };

                // Build up the requests
                int requestNumber{0};
                for (const auto &streamRequest : streamRequests)
                {
                    requestNumber = requestNumber + 1;
                    std::string reason;
                    if (!Utilities::isValid(streamRequest, reason))
                    {
                        reason.append(" for stream request ");
                        reason.append(std::to_string(requestNumber));
                        Finish({grpc::StatusCode::INVALID_ARGUMENT, reason});
                        return;
                    }
                }
                for (const auto &streamRequest : streamRequests)
                {
                    auto requestStartTime
                        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                            streamRequest.start_time());
                    auto requestEndTime
                        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
                            streamRequest.end_time());
                }

                Finish(grpc::Status::OK);
                auto endTime
                    = Utilities::getNow<std::chrono::nanoseconds> ();
                auto elapsedTime
                    = static_cast<double> (endTime.count() - startTime.count())*1.e-9;
            }
        private:
            void OnDone() override
            {
                if (mLogger)
                {
                    SPDLOG_LOGGER_DEBUG(mLogger,
                                        "GetTimeSeries RPC completed");
                }
                delete this;
            }
            void OnCancel() override
            {
                if (mLogger)
                {
                   SPDLOG_LOGGER_DEBUG(mLogger,
                                       "GetTimeSeries RPC canceled");
                }
            }
            std::shared_ptr<spdlog::logger> mLogger{nullptr};
        }; 
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
//        mSubscriberCount.store(0);
//        MetricsSingleton::getInstance().updateSubscribeServiceUtilization(0);
        mStarted = false;
    }

    ~ServiceImpl() override
    {
        stop();
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
 
    ServiceOptions mOptions; 
    std::shared_ptr<StreamDequeMap> mStreamDequeMap{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::unique_ptr<grpc::Server> mServer{nullptr};
    GRPCServerOptions mGRPCOptions;
    std::atomic<bool> mKeepRunning{true};
    bool mSecured{false};
    bool mStarted{false};
};

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
