#ifndef UDATA_PACKET_CACHE_SERVICE_SUBSCRIBER_HPP
#define UDATA_PACKET_CACHE_SERVICE_SUBSCRIBER_HPP
#include <future>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <spdlog/spdlog.h>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}

namespace UDataPacketCacheService
{
 class SubscriberOptions;
}

/// @class Subscriber
/// @brief Defines the gRPC data packet client.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Subscriber
{
public:
    /// @brief Constructor
    Subscriber(const SubscriberOptions &options,
               const std::function<void (UDataPacketServiceAPI::V1::Packet &&paket)> &callback,
               std::shared_ptr<spdlog::logger> logger);

    /// @result True indicates the subscriber is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// @brief Starts the acquisition.
    [[nodiscard]] std::future<void> start();

    /// @brief Destructor
    ~Subscriber();

    Subscriber(const Subscriber &) = delete;
    Subscriber(Subscriber &&) noexcept = delete;
    Subscriber& operator=(const Subscriber &) = delete;
    Subscriber& operator=(Subscriber &&) noexcept = delete;
private:
    class SubscriberImpl;
    std::unique_ptr<SubscriberImpl> pImpl;
};
}
