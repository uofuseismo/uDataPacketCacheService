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
#include "uDataPacketCacheService/circularBufferMap.hpp"
#include "uDataPacketCacheService/circularBuffer.hpp"
#include "uDataPacketCacheService/circularBufferOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

class CircularBufferMap::CircularBufferMapImpl
{
public:
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        auto streamIdentifier = Utilities::toString(packet.stream_identifier());
        auto idx = mCircularBufferMap.find(streamIdentifier);
        if (idx != mCircularBufferMap.end())
        {
            idx->second->addPacket(std::move(packet));
            return;
        }
        // Okay do this the hard way
        CircularBufferOptions options;
        auto circularBuffer
            = std::make_unique<CircularBuffer> (options, std::move(packet));
        auto newNode
            = std::make_pair (streamIdentifier, std::move(circularBuffer));
        auto [jdx, inserted] = mCircularBufferMap.insert(std::move(newNode));
        if (!inserted)
        {
            throw std::runtime_error("Failed to add circular buffer");
        }
    }

    void cleanCircularBuffers()
    {
        auto oldestTime = Utilities::getNow<std::chrono::nanoseconds> () - std::chrono::nanoseconds{ std::chrono::minutes{5} };
        for (auto &it : mCircularBufferMap)
        {
            it.second->removeExpiredPackets(oldestTime);
        }
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::StreamIdentifier> getStreams() const
    {
        std::vector<std::string> streamNames;
        streamNames.reserve(mCircularBufferMap.size());
        for (const auto &idx : mCircularBufferMap)
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

    

    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::mutex mMutex;
    oneapi::tbb::concurrent_map
    <
        std::string,
        std::unique_ptr<CircularBuffer>
    > mCircularBufferMap;
};

/// Destructor
CircularBufferMap::~CircularBufferMap() = default;
