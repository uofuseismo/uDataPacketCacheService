#ifndef UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_MAP_OPTIONS_HPP
#define UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_MAP_OPTIONS_HPP
#include <chrono>
#include <memory>
namespace UDataPacketCacheService
{
/*!
 * @class StreamDequeMapOptions "streamDequeMapOptions.hpp"
 * @brief Defines the options for the stream deque map.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class StreamDequeMapOptions
{
public:
    /// @brief Constructor.
    StreamDequeMapOptions();
    /// @brief Copy constructor.
    StreamDequeMapOptions(const StreamDequeMapOptions &options);
    /// @brief Move constructor.
    StreamDequeMapOptions(StreamDequeMapOptions &&options) noexcept;

    /// @brief Sets the maximum number of packets.
    //void setMaximumNumberOfPackets(int maxPackets);
    /// @result The maximum number of packets.
    /// @note By default this is 150 which, at packets of 2s duration,
    ///       will yield about 300 s (5 minutes) of data.
    //[[nodiscard]] int getMaximumNumberOfPackets() const noexcept;
    
    /// @brief Sets the maximum duration.  The duration is measured from now
    ///        minus this number.  Hence, if the packet end time is less than
    ///        than now - getMaximumDuration() it is purged from the circular
    ///        buffer.
    /// @param[in] maximumDuration   The maximum duration.
    /// @throws std::invalid_argument if the maximum duration is not postivie.
    void setMaximumDuration(const std::chrono::nanoseconds &maximumDuration);
    /// @result The maximum duration.
    /// @note By default this is 5 minutes.
    [[nodiscard]] std::chrono::nanoseconds getMaximumDuration() const noexcept;
    
    /// @brief Destructor.
    ~StreamDequeMapOptions();

    /// @brief Copy assignment.
    StreamDequeMapOptions &operator=(const StreamDequeMapOptions &options);
    /// @brief Move assignment.
    StreamDequeMapOptions &operator=(StreamDequeMapOptions &&options) noexcept;

private:
    class StreamDequeMapOptionsImpl;
    std::unique_ptr<StreamDequeMapOptionsImpl> pImpl;
};
}
#endif
