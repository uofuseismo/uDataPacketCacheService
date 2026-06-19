#include <cstdint>
#include <utility>
#include <optional>
#include <string>
#include <stdexcept>
#include <memory>
#include "uDataPacketCacheService/grpcServerOptions.hpp"

using namespace UDataPacketCacheService;

class GRPCServerOptions::GRPCServerOptionsImpl
{
public:
    std::string mHost{"localhost"};
    uint16_t mPort{50000};
    std::optional<std::string> mAccessToken;
    std::optional<std::string> mServerCertificate;
    std::optional<std::string> mServerKey;
    std::optional<std::string> mClientCertificate;
    bool mReflectionEnabled{false};
};

/// Constructor
GRPCServerOptions::GRPCServerOptions() :
    pImpl(std::make_unique<GRPCServerOptionsImpl>())
{
}

/// Copy constructor
GRPCServerOptions::GRPCServerOptions(const GRPCServerOptions &options)
{
    *this = options;
}

/// Move constructor
GRPCServerOptions::GRPCServerOptions(GRPCServerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment operator
GRPCServerOptions& GRPCServerOptions::operator=(const GRPCServerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<GRPCServerOptionsImpl>(*options.pImpl);
    return *this;
}

/// Move assignment
GRPCServerOptions& GRPCServerOptions::operator=(GRPCServerOptions &&options) noexcept
{
    if (this == &options){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

void GRPCServerOptions::setHost(const std::string &host)
{
    if (host.empty()){throw std::invalid_argument("Host cannot be empty.");}
    pImpl->mHost = host;
}

std::string GRPCServerOptions::getHost() const noexcept
{
    return pImpl->mHost;
}

void GRPCServerOptions::setPort(uint16_t port)
{
    if (port == 0){throw std::invalid_argument("Port cannot be 0.");}
    pImpl->mPort = port;
}

uint16_t GRPCServerOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

void GRPCServerOptions::setAccessToken(const std::string &accessToken)
{
    if (accessToken.empty())
    {
        throw std::invalid_argument("Access token cannot be empty.");
    }
    pImpl->mAccessToken = std::optional<std::string> (accessToken);
}

std::optional<std::string> GRPCServerOptions::getAccessToken() const noexcept
{
    return pImpl->mAccessToken;
}

void GRPCServerOptions::setServerCertificate(
    const std::string &serverCertificate)
{
    if (serverCertificate.empty())
    {
        throw std::invalid_argument("Server certificate cannot be empty.");
    }
    pImpl->mServerCertificate = std::optional<std::string> (serverCertificate);
}

std::optional<std::string> 
    GRPCServerOptions::getServerCertificate() const noexcept
{
    return pImpl->mServerCertificate;
}

void GRPCServerOptions::setServerKey(const std::string &serverKey)
{
    if (serverKey.empty())
    {
        throw std::invalid_argument("Server key cannot be empty.");
    }
    pImpl->mServerKey = std::optional<std::string> (serverKey);
}

std::optional<std::string> GRPCServerOptions::getServerKey() const noexcept
{
    return pImpl->mServerKey;
}

void GRPCServerOptions::setClientCertificate(
    const std::string &clientCertificate)
{
    if (clientCertificate.empty())
    {
        throw std::invalid_argument("Client certificate cannot be empty.");
    }
    pImpl->mClientCertificate = std::optional<std::string> (clientCertificate);
}

std::optional<std::string> 
    GRPCServerOptions::getClientCertificate() const noexcept
{
    return pImpl->mClientCertificate;
}

void GRPCServerOptions::enableReflection() noexcept
{
    pImpl->mReflectionEnabled = true;
}

void GRPCServerOptions::disableReflection() noexcept
{
    pImpl->mReflectionEnabled = false;
}

bool GRPCServerOptions::isReflectionEnabled() const noexcept
{
    return pImpl->mReflectionEnabled;
}

GRPCServerOptions::~GRPCServerOptions() = default;
