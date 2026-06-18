#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#include "uDataPacketCacheService/circularBufferOptions.hpp"

using namespace UDataPacketCacheService;

class CircularBufferOptions::CircularBufferOptionsImpl
{
public:
    // 5 minutes (300 s).  Assume 2s packets (generous).
    // Then there would be 150 packets.
    int mMaximumNumberOfPackets{150};
    //std::chrono::nanoseconds mMaximumDuration{std::chrono::minutes {5}};
};

/// Constructor
CircularBufferOptions::CircularBufferOptions() :
    pImpl(std::make_unique<CircularBufferOptionsImpl> ())
{
}

/// Copy constructor
CircularBufferOptions::CircularBufferOptions(
    const CircularBufferOptions &options)
{
    *this = options;
}

/// Move constructor
CircularBufferOptions::CircularBufferOptions(
    CircularBufferOptions &&options) noexcept
{
    *this = std::move(options);
}

/*
/// Max duration
void CircularBufferOptions::setMaximumDuration(
    const std::chrono::nanoseconds &duration)
{
    constexpr std::chrono::nanoseconds one{1};
    if (duration < one)
    {
        throw std::invalid_argument("Duration must be positive");
    }
    pImpl->mMaximumDuration = duration;
}

std::chrono::nanoseconds 
CircularBufferOptions::getMaximumDuration() const noexcept
{
    return pImpl->mMaximumDuration;
}
*/

/// Max packets
void CircularBufferOptions::setMaximumNumberOfPackets(const int maxPackets)
{
    if (maxPackets <= 0)
    {
        throw std::invalid_argument("Max number of packets must be positive");
    }
    pImpl->mMaximumNumberOfPackets = maxPackets;
}

int CircularBufferOptions::getMaximumNumberOfPackets() const noexcept
{
    return pImpl->mMaximumNumberOfPackets;
}

/// Copy assignment
CircularBufferOptions& 
CircularBufferOptions::operator=(const CircularBufferOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<CircularBufferOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
CircularBufferOptions& 
CircularBufferOptions::operator=(CircularBufferOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
CircularBufferOptions::~CircularBufferOptions() = default;
