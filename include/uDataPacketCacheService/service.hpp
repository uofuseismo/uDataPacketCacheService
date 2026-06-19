#ifndef UDATA_PACKET_CACHE_SERVICE_SERVICE_HPP
#define UDATA_PACKET_CACHE_SERVICE_SERVICE_HPP
#include <future>
#include <memory>
namespace UDataPacketCacheService
{
 class CircularBufferMap;
 class ServiceOptions;
}
namespace UDataPacketCacheService
{
/// @class Service service.hpp
/// @brief The actual gRPC service that serves packets.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Service
{
public:
    /// @brief Creates the service.
    Service(std::shared_ptr<CircularBufferMap> circularBufferMap,
            const ServiceOptions &options);  

    /// @brief Runs the service on a separate thread.
    std::future<void> start();

    /// @brief Stops the service.
    void stop();

    /// @brief Destructor.
    ~Service(); 
 
    Service() = delete;
    Service(const Service &) = delete;
    Service(Service &&) noexcept = delete;
    Service& operator=(const Service &) = delete;
    Service& operator=(Service &&) noexcept = delete;
private:
    class ServiceImpl;
    std::unique_ptr<ServiceImpl> pImpl;
};
}
#endif
