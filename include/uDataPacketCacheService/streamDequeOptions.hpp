#ifndef UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_OPTIONS_HPP
#define UDATA_PACKET_CACHE_SERVICE_STREAM_DEQUE_OPTIONS_HPP
#include <memory>
#include <chrono>
namespace UDataPacketCacheService
{
/*!
 * @class StreamDequeOptions "streamDequeOptions.hpp" 
 * @brief Defines the options for a seismic data stream's deque.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class StreamDequeOptions
{
public:
    /// @brief Constructor.
    StreamDequeOptions();
    /// @brief Copy constructor.
    StreamDequeOptions(const StreamDequeOptions &options);
    /// @brief Move constructor.
    StreamDequeOptions(StreamDequeOptions &&options) noexcept;

    /// @brief Sets the maximum number of packets.
    /// @note This is a safety mechanism that can ultimately do more harm than
    ///       good.  Nominally, we rely on the retention duration to control
    ///       this number.
    void setMaximumNumberOfPackets(int maxPackets);
    /// @result The maximum number of packets.
    /// @note By default this is effectively unlimited.
    [[nodiscard]] int getMaximumNumberOfPackets() const noexcept;
    
    /// @brief Sets the maximum duration.  The duration is measured from now
    ///        minus this number.  Hence, if the packet end time is less than
    ///        than now - getMaximumDuration() it is purged from the deque.
    /// @param[in] maximumDuration   The maximum duration.
    /// @throws std::invalid_argument if the maximum duration is not postivie.
    //void setMaximumDuration(const std::chrono::nanoseconds &maximumDuration);
    /// @result The maximum duration.
    /// @note By default this is 5 minutes.
    //[[nodiscard]] std::chrono::nanoseconds getMaximumDuration() const noexcept;
    
    /// @brief Destructor.
    ~StreamDequeOptions();

    /// @brief Copy assignment.
    StreamDequeOptions &operator=(const StreamDequeOptions &options);
    /// @brief Move assignment.
    StreamDequeOptions &operator=(StreamDequeOptions &&options) noexcept;
private:
    class StreamDequeOptionsImpl;
    std::unique_ptr<StreamDequeOptionsImpl> pImpl;
};
}
#endif
