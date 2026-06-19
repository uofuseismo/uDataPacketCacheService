#ifndef UDATA_PACKET_CACHE_SERVICE_SERVICE_HPP
#define UDATA_PACKET_CACHE_SERVICE_SERVICE_HPP
#include <memory>
namespace UDataPacketCacheService
{
 class CircularBufferMap;
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
    Service(std::shared_ptr<CircularBufferMap> circularBufferMap);  
     
    Service() = delete;
    Service(const Service &) = delete;
    Service& operator=(const Service &) = delete;
private:
    class ServiceImpl;
    std::unique_ptr<ServiceImpl> pImpl;
};
}
#endif
