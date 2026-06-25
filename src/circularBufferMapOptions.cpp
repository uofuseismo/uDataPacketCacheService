#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#include "uDataPacketCacheService/circularBufferMapOptions.hpp"

using namespace UDataPacketCacheService;

class CircularBufferMapOptions::CircularBufferMapOptionsImpl
{
public:
    // Maximum duration.
    std::chrono::nanoseconds mMaximumDuration{std::chrono::minutes {5}};
};

/// Constructor
CircularBufferMapOptions::CircularBufferMapOptions() :
    pImpl(std::make_unique<CircularBufferMapOptionsImpl> ())
{
}

/// Copy constructor
CircularBufferMapOptions::CircularBufferMapOptions(
    const CircularBufferMapOptions &options)
{
    *this = options;
}

/// Move constructor
CircularBufferMapOptions::CircularBufferMapOptions(
    CircularBufferMapOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Max duration
void CircularBufferMapOptions::setMaximumDuration(
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
CircularBufferMapOptions::getMaximumDuration() const noexcept
{
    return pImpl->mMaximumDuration;
}

/*
/// Max packets
void CircularBufferMapOptions::setMaximumNumberOfPackets(const int maxPackets)
{
    if (maxPackets <= 0)
    {
        throw std::invalid_argument("Max number of packets must be positive");
    }
    pImpl->mMaximumNumberOfPackets = maxPackets;
}

int CircularBufferMapOptions::getMaximumNumberOfPackets() const noexcept
{
    return pImpl->mMaximumNumberOfPackets;
}
*/

/// Copy assignment
CircularBufferMapOptions& 
CircularBufferMapOptions::operator=(const CircularBufferMapOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<CircularBufferMapOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
CircularBufferMapOptions& 
CircularBufferMapOptions::operator=(
    CircularBufferMapOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
CircularBufferMapOptions::~CircularBufferMapOptions() = default;
