#ifndef UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_MAP_HPP
#define UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_MAP_HPP
#include <vector>
#include <memory>

namespace UDataPacketCacheServiceAPI::V1
{
 class Packet;
 class StreamIdentifier; 
}
namespace UDataPacketServiceAPI::V1
{
 class Packet;
} 

namespace UDataPacketCacheService
{
/*!
 * @class CircularBufferMap "circularBufferMap.hpp"
 * @brief This is a thread-safe class for managing the circular buffers.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class CircularBufferMap
{
public:
CircularBufferMap();
    /// @brief Adds a packet to the map.
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet);

    /// @result True indicates the stream exists.
    [[nodiscard]] bool exists(const std::string &identifier) const noexcept;
    /// @result True indicates the stream exists.
    [[nodiscard]] bool exists(const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier);

    /// @brief Gets the packets with data between the desired start and end
    ///        time corresponding to the desired stream.  Note, there may be
    ///        some data that arrives before and some data that arrives after
    ///        the desired times as the packets are not trimmed.
    /// @param[in] identifier       The stream identiifer.
    /// @param[in] startAndEndTime  The time range to query.
    /// @result The corresponding packets in the given time range.
    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet>
         getPackets(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
                    const std::pair<std::chrono::nanoseconds,
                                    std::chrono::nanoseconds> &startAndEndTime) const;
    /// @brief Destructor.
    ~CircularBufferMap();
    CircularBufferMap(const CircularBufferMap &) = delete;
    CircularBufferMap& operator=(const CircularBufferMap &) = delete;
    
private:
    class CircularBufferMapImpl;
    std::unique_ptr<CircularBufferMapImpl> pImpl;
};
}
#endif
