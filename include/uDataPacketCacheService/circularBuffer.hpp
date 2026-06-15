#ifndef UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_HPP
#define UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_HPP
#include <memory>
namespace UDataPacketCacheService
{
/*!
 * @brief This is a non-thread-safe class that stores seismic data packets.
 */
class CircularBuffer
{
public:
private:
    class CircularBufferImpl;
    std::unique_ptr<CircularBufferImpl> pImpl;
};
}
#endif
