#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include "uDataPacketCacheService/streamDequeOptions.hpp"

using namespace UDataPacketCacheService;

class StreamDequeOptions::StreamDequeOptionsImpl
{
public:
    // Effectively unlimited.
    int mMaximumNumberOfPackets{std::numeric_limits<int>::max()};
    //std::chrono::nanoseconds mMaximumDuration{std::chrono::minutes {5}};
};

/// Constructor
StreamDequeOptions::StreamDequeOptions() :
    pImpl(std::make_unique<StreamDequeOptionsImpl> ())
{
}

/// Copy constructor
StreamDequeOptions::StreamDequeOptions(
    const StreamDequeOptions &options)
{
    *this = options;
}

/// Move constructor
StreamDequeOptions::StreamDequeOptions(
    StreamDequeOptions &&options) noexcept
{
    *this = std::move(options);
}

/*
/// Max duration
void StreamDequeOptions::setMaximumDuration(
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
StreamDequeOptions::getMaximumDuration() const noexcept
{
    return pImpl->mMaximumDuration;
}
*/

/// Max packets
void StreamDequeOptions::setMaximumNumberOfPackets(const int maxPackets)
{
    if (maxPackets <= 0)
    {
        throw std::invalid_argument("Max number of packets must be positive");
    }
    pImpl->mMaximumNumberOfPackets = maxPackets;
}

int StreamDequeOptions::getMaximumNumberOfPackets() const noexcept
{
    return pImpl->mMaximumNumberOfPackets;
}

/// Copy assignment
StreamDequeOptions& 
StreamDequeOptions::operator=(const StreamDequeOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<StreamDequeOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
StreamDequeOptions& 
StreamDequeOptions::operator=(StreamDequeOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
StreamDequeOptions::~StreamDequeOptions() = default;
