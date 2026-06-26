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
        auto [jdx, inserted] = mStreamDequeMap.insert(std::move(newNode));
        if (!inserted)
        {
            throw std::runtime_error("Failed to add stream deque for "
                                   + streamIdentifier);
        }
    }

    void cleanStreamDeques()
    {
        auto oldestTime = Utilities::getNow<std::chrono::nanoseconds> ()
                        - mOptions.getMaximumDuration();
        for (auto &it : mStreamDequeMap)
        {
            it.second->removeExpiredPackets(oldestTime);
        }
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> getStreams() const
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

/// Destructor
StreamDequeMap::~StreamDequeMap() = default;
