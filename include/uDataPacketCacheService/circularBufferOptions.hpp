#ifndef UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_OPTIONS_HPP
#define UDATA_PACKET_CACHE_SERVICE_CIRCULAR_BUFFER_OPTIONS_HPP
#include <memory>
#include <optional>
#include <chrono>
namespace UDataPacketCacheService
{
/*!
 * @class CircularBufferOptions "circularBufferOptions.hpp" 
 * @brief Defines the options for the circular buffers.
 * @copyright Ben Baker (University of Utah) distributed under the
 *            MIT NO AI license.
 */
class CircularBufferOptions
{
public:
    /// @brief Constructor.
    CircularBufferOptions();
    /// @brief Copy constructor.
    CircularBufferOptions(const CircularBufferOptions &options);
    /// @brief Move constructor.
    CircularBufferOptions(CircularBufferOptions &&options) noexcept;

    /// @brief Sets the maximum number of packets.
    void setMaximumNumberOfPackets(int maxPackets);
    /// @result The maximum number of packets.
    /// @note By default this is 150 which, at packets of 2s duration,
    ///       will yield about 300 s (5 minutes) of data.
    [[nodiscard]] int getMaximumNumberOfPackets() const noexcept;
    
    /// @brief Sets the maximum duration.  The duration is measured from now
    ///        minus this number.  Hence, if the packet end time is less than
    ///        than now - getMaximumDuration() it is purged from the circular
    ///        buffer.
    /// @param[in] maximumDuration   The maximum duration.
    /// @throws std::invalid_argument if the maximum duration is not postivie.
    //void setMaximumDuration(const std::chrono::nanoseconds &maximumDuration);
    /// @result The maximum duration.
    /// @note By default this is 5 minutes.
    //[[nodiscard]] std::chrono::nanoseconds getMaximumDuration() const noexcept;
    
    /// @brief Destructor.
    ~CircularBufferOptions();

    /// @brief Copy assignment.
    CircularBufferOptions &operator=(const CircularBufferOptions &options);
    /// @brief Move assignment.
    CircularBufferOptions &operator=(CircularBufferOptions &&options) noexcept;
private:
    class CircularBufferOptionsImpl;
    std::unique_ptr<CircularBufferOptionsImpl> pImpl;
};
}
#endif
