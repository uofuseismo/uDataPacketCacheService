#ifndef UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_HPP
#define UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_HPP
#include <memory>
#include <string>
#include <vector>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}
namespace UDataPacketCacheServiceAPI::V1
{
 class Packet;
 class StreamIdentifier;
}
namespace UDataPacketCacheService
{
 class CircularBufferOptions;
}
namespace UDataPacketCacheService
{
/*!
 * @class CircularBuffer "circularBuffer.hpp"
 * @brief This is a non-thread-safe class that stores seismic data packets.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class CircularBuffer
{
public:
    /// @brief Constructor.
    CircularBuffer(const CircularBufferOptions &options,
                   UDataPacketServiceAPI::V1::Packet &&packet);
    CircularBuffer(const CircularBufferOptions &options,
                   const UDataPacketServiceAPI::V1::Packet &packet);

    /// @brief Adds a packet.
    /// @param[in,out] packet  The packet to add to the circular buffer.
    ///                        On exit, packet's behavior is undefined.
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet);
    /// @brief Adds a packet.
    /// @param[in] packet  The packet to add.
    void addPacket(const UDataPacketServiceAPI::V1::Packet &packet);
    /// @result The stream identifier.
    [[nodiscard]] std::string getIdentifier() const noexcept;

    /// @brief Removes packets that have no data exceeding the given time.
    /// @param[in] oldestTime   If the packet's end time is less than this
    ///                         then it will be removed.
    /// @result The number of packets deleted.
    void removeExpiredPackets(const std::chrono::nanoseconds &oldestTime);
    
    /// @brief Gets the packets with data between the desired start and end
    ///        time.  Note, there may be some data that arrives before and
    ///        some data that arrives after the desired times as the
    ///        packets are not trimmed.
    /// @param[in] startAndEndTime  The time range to query.
    /// @result The corresponding packets in the given time range.
    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet>
         getPackets(const std::pair<std::chrono::nanoseconds,
                                    std::chrono::nanoseconds> &startAndEndTime) const;
    /// @result All the packets in the circular buffer
    /// @note This is a debugging function.
    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet> getAllPackets() const;

    /// @brief Destructor.
    ~CircularBuffer();

    CircularBuffer() = delete;
    CircularBuffer& operator=(const CircularBuffer &) = delete;
    CircularBuffer& operator=(CircularBuffer &&) noexcept = delete;
private:
    class CircularBufferImpl;
    std::unique_ptr<CircularBufferImpl> pImpl;
};
}
#endif
