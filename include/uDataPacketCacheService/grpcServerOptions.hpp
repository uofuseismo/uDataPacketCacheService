#ifndef UDATA_PACKET_CACHE_SERVICE_GRPC_SERVER_OPTIONS_HPP
#define UDATA_PACKET_CACHE_SERVICE_GRPC_SERVER_OPTIONS_HPP
#include <cstdint>
#include <string>
#include <optional>
#include <memory>
namespace UDataPacketCacheService
{
/// @class GRPCServerOptions "grpcServerOptions.hpp"
/// @brief This class is used to configure the gRPC server options.
/// @copyright Ben Baker (University of Utah) distributed under the
///             MIT NO AI license.
class GRPCServerOptions
{
public:
    /// @brief Constructor.
    GRPCServerOptions();
    /// @brief Copy constructor.
    GRPCServerOptions(const GRPCServerOptions &options);
    /// @brief Move constructor.
    GRPCServerOptions(GRPCServerOptions &&options) noexcept;

    /// @brief Sets the host.
    /// @param host The host to set.
    void setHost(const std::string &host);
    /// @brief Gets the host.
    /// @return The host.
    /// @note By default this is localhost.
    [[nodiscard]] std::string getHost() const noexcept;

    /// @brief Sets the port.
    /// @param port The port to set.
    void setPort(uint16_t port);
    /// @brief Gets the port.
    /// @return The port.
    /// @note By default this is 50000.
    [[nodiscard]] uint16_t getPort() const noexcept;

    /// @brief Sets the access token.
    /// @param accessToken The access token to set.
    /// @note Access tokens can only be used by gRPC if the server certificate
    ///       and server key are set.
    void setAccessToken(const std::string &accessToken);
    /// @brief Gets the access token.
    /// @return The access token.  If this is not set, then std::nullopt is returned.
    [[nodiscard]] std::optional<std::string> getAccessToken() const noexcept;

    /// @brief Sets the server certificate.
    /// @param serverCertificate  The server certificate to set.
    void setServerCertificate(const std::string &serverCertificate);
    /// @brief Gets the server certificate.
    /// @return The server certificate.  If this is not set, then std::nullopt is returned.
    [[nodiscard]] std::optional<std::string> getServerCertificate() const noexcept;

    /// @brief Sets the server's private key.
    /// @param serverKey The server key to set.
    void setServerKey(const std::string &serverKey);
    /// @brief Gets the server's private key.
    /// @return The server's private key.  If this is not set, then std::nullopt is returned.
    [[nodiscard]] std::optional<std::string> getServerKey() const noexcept;

    /// @brief Sets the client certificate.
    /// @param clientCertificate  The client certificate to set.
    void setClientCertificate(const std::string &clientCertificate);
    /// @brief Gets the client certificate.
    /// @return The client certificate.  If this is not set, then std::nullopt is returned.
    [[nodiscard]] std::optional<std::string> getClientCertificate() const noexcept;

    /// @brief Enables reflection.
    void enableReflection() noexcept;
    /// @brief Disables reflection.
    void disableReflection() noexcept;
    /// @brief Gets whether reflection is enabled.
    /// @return True if reflection is enabled, false otherwise.  By default this is false.
    [[nodiscard]] bool isReflectionEnabled() const noexcept;

    /// @brief Destructor.
    ~GRPCServerOptions();
    /// @brief Copy assignment operator.
    GRPCServerOptions& operator=(const GRPCServerOptions &options);
    /// @brief Move assignment operator.
    GRPCServerOptions& operator=(GRPCServerOptions &&options) noexcept;
private:
    class GRPCServerOptionsImpl;
    std::unique_ptr<GRPCServerOptionsImpl> pImpl;
};
}
#endif
