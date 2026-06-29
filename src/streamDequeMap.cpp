#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <oneapi/tbb/concurrent_map.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketCacheServiceAPI/v1/packet.pb.h>
#include <uDataPacketCacheServiceAPI/v1/stream_identifier.pb.h>
#include "uDataPacketCacheService/streamDequeMap.hpp"
#include "uDataPacketCacheService/streamDequeMapOptions.hpp"
#include "uDataPacketCacheService/streamDeque.hpp"
#include "uDataPacketCacheService/streamDequeOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

class StreamDequeMap::StreamDequeMapImpl
{
public:
    StreamDequeMapImpl(const StreamDequeMapOptions &options,
                       std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            auto classId
                = std::to_string (reinterpret_cast<std::uintptr_t> (this));
            mLogger = spdlog::stdout_color_mt("StreamDequeConsole-" + classId);
            // NOLINTEND(misc-include-cleaner)
        }
    }

    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        auto streamIdentifier = Utilities::toString(packet.stream_identifier());
        auto idx = mStreamDequeMap.find(streamIdentifier);
        if (idx != mStreamDequeMap.end())
        {
            idx->second->addPacket(std::move(packet));
            return;
        }
        // Okay do this the hard way
        StreamDequeOptions options;
        auto streamDeque
            = std::make_unique<StreamDeque> (options, std::move(packet));
        auto newNode
            = std::make_pair (streamIdentifier, std::move(streamDeque));
        // N.B. this is a concurrently safe modifier
        auto [jdx, inserted] = mStreamDequeMap.insert(std::move(newNode));
        if (!inserted)
        {
            throw std::runtime_error("Failed to add stream deque for "
                                   + streamIdentifier);
        }
        SPDLOG_LOGGER_INFO(mLogger,
                           "Added {} to deque map",
                           streamIdentifier);
    }

    void removeExpiredPackets()
    {
        auto oldestTime = Utilities::getNow<std::chrono::nanoseconds> ()
                        - mOptions.getMaximumDuration();
        for (auto &it : mStreamDequeMap)
        {
            it.second->removeExpiredPackets(oldestTime);
        }
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet>
         getPackets(const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier,
                    const std::pair<std::chrono::nanoseconds,
                                    std::chrono::nanoseconds> &startAndEndTime) const
    {
        auto streamIdentifier = Utilities::toString(identifier);
        std::vector<UDataPacketCacheServiceAPI::V1::Packet> result;
        auto idx = mStreamDequeMap.find(streamIdentifier);
        if (idx != mStreamDequeMap.end())
        {
            result = idx->second->getPackets(startAndEndTime);
        }
        return result; 
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> getAvailableStreams() const
    {
        std::vector<std::string> streamNames;
        streamNames.reserve(mStreamDequeMap.size());
        for (const auto &idx : mStreamDequeMap)
        {
            streamNames.push_back(idx.first); 
        }
        // Now build the streams
        std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> result;
        result.reserve(streamNames.size());
        std::set<std::string> streamNamesSet;
        for (const auto &streamName : streamNames)
        {
            if (!streamNamesSet.contains(streamName))
            {
                auto identifier
                    = Utilities::fromString
                      <UDataPacketCacheServiceAPI::V1::StreamIdentifier>
                      (streamName);
                result.push_back(std::move(identifier));
                streamNamesSet.insert(streamName);
            }
        }
        return result;
    }
//public: 
    StreamDequeMapOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::mutex mMutex;
    oneapi::tbb::concurrent_map
    <
        std::string,
        std::unique_ptr<StreamDeque>
    > mStreamDequeMap;
};

StreamDequeMap::StreamDequeMap(
    const StreamDequeMapOptions &options,
    std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<StreamDequeMapImpl> (options, std::move(logger)))
{
}

/// Add a packet to the map
void StreamDequeMap::addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
{
    pImpl->addPacket(std::move(packet));
}

void StreamDequeMap::addPacket(
    const UDataPacketServiceAPI::V1::Packet &packetIn)
{
    auto packet = packetIn;
    addPacket(std::move(packet));
}

/// Remove expired packets
void StreamDequeMap::removeExpiredPackets()
{
    pImpl->removeExpiredPackets();
}

std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> 
    StreamDequeMap::getAvailableStreams() const
{
    return pImpl->getAvailableStreams();
}

/// Get the packets
std::vector<UDataPacketCacheServiceAPI::V1::Packet>
StreamDequeMap::getPackets(
    const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier,
    const std::pair<std::chrono::nanoseconds,
                    std::chrono::nanoseconds> &startAndEndTime) const
{
    if (!identifier.has_network())
    {
        throw std::invalid_argument("Network not set");
    }
    if (!identifier.has_station())
    {
        throw std::invalid_argument("Station not set");
    }
    if (!identifier.has_channel())
    {
        throw std::invalid_argument("Channel not set");
    }
    if (!identifier.has_location_code())
    {
        throw std::invalid_argument("Location code not set");
    } 
    if (startAndEndTime.first > startAndEndTime.second)
    {
        throw std::invalid_argument("Start time is greater than end time");
    }
    return pImpl->getPackets(identifier, startAndEndTime);
}


/// Destructor
StreamDequeMap::~StreamDequeMap() = default;
