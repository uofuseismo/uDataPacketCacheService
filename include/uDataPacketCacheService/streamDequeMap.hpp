#ifndef UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_MAP_HPP
#define UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_MAP_HPP
#include <cstdint>
#include <memory>
#include <vector>
#include <spdlog/logger.h>

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
 class StreamDequeMapOptions;
}
namespace UDataPacketCacheService
{
/*!
 * @class StreamDequeMap "streamDequeMap.hpp"
 * @brief This is a thread-safe class for managing the individual stream
 *        deques.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class StreamDequeMap
{
public:
    /// @brief Constructor.
    /// @param[in] options  The options defining the containers behavior.
    /// @param[in] logger   The logging utility.
    StreamDequeMap(const StreamDequeMapOptions &options,
                   std::shared_ptr<spdlog::logger> logger);
    
    /// @brief Adds a packet to the map.
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet);
    void addPacket(const UDataPacketServiceAPI::V1::Packet &packet);

    /// @brief Cleans out expired packets from the stream deques.
    uint32_t removeExpiredPackets();

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
         getPackets(const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier,
                    const std::pair<std::chrono::nanoseconds,
                                    std::chrono::nanoseconds> &startAndEndTime) const;

    /// @result Gets the available streams in this container.
    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> getAvailableStreams() const;

    /// @brief Destructor.
    ~StreamDequeMap();
    StreamDequeMap(const StreamDequeMap &) = delete;
    StreamDequeMap& operator=(const StreamDequeMap &) = delete;
    
private:
    class StreamDequeMapImpl;
    std::unique_ptr<StreamDequeMapImpl> pImpl;
};
}
#endif
