#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#include "uDataPacketCacheService/streamDequeMapOptions.hpp"

using namespace UDataPacketCacheService;

class StreamDequeMapOptions::StreamDequeMapOptionsImpl
{
public:
    // Maximum duration.
    std::chrono::nanoseconds mMaximumDuration{std::chrono::minutes {5}};
};

/// Constructor
StreamDequeMapOptions::StreamDequeMapOptions() :
    pImpl(std::make_unique<StreamDequeMapOptionsImpl> ())
{
}

/// Copy constructor
StreamDequeMapOptions::StreamDequeMapOptions(
    const StreamDequeMapOptions &options)
{
    *this = options;
}

/// Move constructor
StreamDequeMapOptions::StreamDequeMapOptions(
    StreamDequeMapOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Max duration
void StreamDequeMapOptions::setMaximumDuration(
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
StreamDequeMapOptions::getMaximumDuration() const noexcept
{
    return pImpl->mMaximumDuration;
}

/*
/// Max packets
void StreamDequeMapOptions::setMaximumNumberOfPackets(const int maxPackets)
{
    if (maxPackets <= 0)
    {
        throw std::invalid_argument("Max number of packets must be positive");
    }
    pImpl->mMaximumNumberOfPackets = maxPackets;
}

int StreamDequeMapOptions::getMaximumNumberOfPackets() const noexcept
{
    return pImpl->mMaximumNumberOfPackets;
}
*/

/// Copy assignment
StreamDequeMapOptions& 
StreamDequeMapOptions::operator=(const StreamDequeMapOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<StreamDequeMapOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
StreamDequeMapOptions& 
StreamDequeMapOptions::operator=(
    StreamDequeMapOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
StreamDequeMapOptions::~StreamDequeMapOptions() = default;
