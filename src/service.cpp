#include <atomic>
#include <cstdint>
#include <future>
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
            Reactor(const UDataPacketCacheServiceAPI::V1::DataRequest &request,
                    UDataPacketCacheServiceAPI::V1::DataResponse *response,
                    StreamDequeMap &streamDequeMap,
                    std::shared_ptr<spdlog::logger> logger) :
                mLogger(std::move(logger))
            {
                Finish(grpc::Status::OK);
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
        return new Reactor(*request, response, *mStreamDequeMap, mLogger);
    }

    void start()
    {
        mKeepRunning.store(true);

        const auto grpcOptions = mOptions.getGRPCOptions();
        const auto address = grpcOptions.getHost() + ":"
                           + std::to_string(grpcOptions.getPort());
        grpc::ServerBuilder builder;
        if (grpcOptions.getServerKey() == std::nullopt ||
            grpcOptions.getServerCertificate() == std::nullopt)
        {        
            SPDLOG_LOGGER_INFO(mLogger,
                               "Initiating non-secured subscribe service");
            builder.AddListeningPort(address,
                                     grpc::InsecureServerCredentials());
            mSecured = false;
        }
        else
        {
            auto serverKey = grpcOptions.getServerKey();
            auto serverCertificate = grpcOptions.getServerCertificate();
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
