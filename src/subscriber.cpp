#include <atomic>
#include <future>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <condition_variable>
#include <memory>
#include <exception>
#include <stdexcept>
#include <functional>
#include <utility>
#include <map>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/config.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/string_ref.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/auth_context.h>
#include <oneapi/tbb/concurrent_map.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/subscription_request.pb.h>
#include <uDataPacketServiceAPI/v1/broadcast.grpc.pb.h>
#include "uDataPacketCacheService/subscriber.hpp"
#include "uDataPacketCacheService/subscriberOptions.hpp"
#include "uDataPacketCacheService/grpcClientOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

namespace
{

class CustomAuthenticator : public grpc::MetadataCredentialsPlugin
{
public:
    CustomAuthenticator(const grpc::string &token) :
        mToken(token)
    {
    }
    grpc::Status GetMetadata(
        grpc::string_ref, // serviceURL, 
        grpc::string_ref, // methodName,
        const grpc::AuthContext &,//channelAuthContext,
        std::multimap<grpc::string, grpc::string> *metadata) override
    {
        metadata->insert(std::make_pair("x-custom-auth-token", mToken));
        return grpc::Status::OK;
    }
//private:
    grpc::string mToken;
};

std::shared_ptr<grpc::Channel>
    createChannel(const auto &options,
                  spdlog::logger *logger)
{
    auto address = UDataPacketCacheService::makeAddress(options);
    auto serverCertificate = options.getServerCertificate();
    if (serverCertificate)
    {
#ifndef NDEBUG
        assert(!serverCertificate->empty());
#endif
        if (options.getAccessToken())
        {
            auto apiKey = *options.getAccessToken();
#ifndef NDEBUG
            assert(!apiKey.empty());
#endif
            SPDLOG_LOGGER_INFO(logger,
                               "Creating secure channel with API key to {}",
                               address);
            auto callCredentials = grpc::MetadataCredentialsFromPlugin(
                std::unique_ptr<grpc::MetadataCredentialsPlugin> (
                    new ::CustomAuthenticator(apiKey)));
            grpc::SslCredentialsOptions sslOptions;
            sslOptions.pem_root_certs = *serverCertificate;
            auto channelCredentials
                = grpc::CompositeChannelCredentials(
                      grpc::SslCredentials(sslOptions),
                      callCredentials);
            return grpc::CreateChannel(address, channelCredentials);
        }
        SPDLOG_LOGGER_INFO(logger,
                           "Creating secure channel without API key to {}",
                           address);
        grpc::SslCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = *serverCertificate;
        return grpc::CreateChannel(address,
                                   grpc::SslCredentials(sslOptions));
     }
     SPDLOG_LOGGER_INFO(logger,
                        "Creating non-secure channel to {}",
                         address);
     return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

}

