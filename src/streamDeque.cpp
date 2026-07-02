#include <iomanip>
#include <iostream>
#include <algorithm>
#include <atomic>
#ifndef NDEBUG
#include <cassert>
#endif
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <google/protobuf/util/time_util.h>
#include <uDataPacketCacheServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include "uDataPacketCacheService/streamDeque.hpp"
#include "uDataPacketCacheService/streamDequeOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

namespace
{

struct PacketTime
{
    UDataPacketCacheServiceAPI::V1::Packet packet;
    std::chrono::nanoseconds startTime;
    std::chrono::nanoseconds endTime;
};

}

class StreamDeque::StreamDequeImpl
{
public:
    explicit StreamDequeImpl(const StreamDequeOptions &options) :
        mOptions(options)
    {
        mMaximumNumberOfPackets = mOptions.getMaximumNumberOfPackets(); 
        mLastUpdate.store(
            Utilities::getNow<std::chrono::microseconds> (), 
            std::memory_order::seq_cst);
    }

    void addPacket(UDataPacketCacheServiceAPI::V1::Packet &&packet)
    {
        // Easier to work with than .first and and .second
        const auto startTime
            = Utilities::getStartTime<std::chrono::nanoseconds> (packet);
        const auto endTime
            = Utilities::getEndTime<std::chrono::nanoseconds> (packet);

        // Get ready to insert it
        ::PacketTime newPacket
        {
             std::move(packet),
             startTime,
             endTime
        };

        // Empty buffer - hey, free money!
        if (mDeque.empty())
        {
            mDeque.push_back(std::move(newPacket));
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst);
            return;
        }

        const auto isFull
            = mDeque.size() >= mMaximumNumberOfPackets ? true : false;

        // Almost always, we will be inserting at the end
        const auto currentOldestTime = mDeque.back().endTime;
        if (startTime >= currentOldestTime)
        {
            if (isFull){mDeque.pop_front();}
            mDeque.push_back(std::move(newPacket));
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst);
            return;
        }
        // There's a wonky case where this can happen.  The latest
        // packet's timing slipped just a bit.  It's still the most recent.
        if (startTime > mDeque.back().startTime)
        {
            if (isFull){mDeque.pop_front();}
            mDeque.push_back(std::move(newPacket));
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst);
            return;
        }
        // Maybe we can insert at the beginning
        const auto currentEarliestTime = (mDeque.front().startTime);
        // For the same reason as below - we assume this data is already
        // acquired and skip it (duplicate or less likely a GPS slip).
        if (startTime == currentEarliestTime)
        {
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst); 
            return;
        }
        // The buffer is full and we need to prioritize new data so skip this.
        // Still, we can mark it as `updated' to demonstrate this stream is
        // still active.
        if (startTime < currentEarliestTime && isFull)
        {
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst);
            return;
        }
        // Okay, there's space at the beginning so accomodate it.
        // N.B. it must also be true that startTime < currentEarliestTime
        if (endTime <= currentEarliestTime && !isFull) 
        {
            mDeque.push_front(std::move(newPacket));
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                std::memory_order::seq_cst);
            return;
        }

        auto it
            = std::ranges::upper_bound(mDeque,
                                       newPacket,
                                       [](const auto &element, const auto &query)
              {
                  return element.startTime <= query.startTime;
              });
        if (it != mDeque.end())
        {
            // We have an exact match.  Do nothing because it is either
            // a backfill (same thing) or it's a timing error which in 
            // the near-real time context more likely indicates a problem.
            const auto startTimeNeighbor = it->startTime;
            if (startTime == startTimeNeighbor)
            {
                //mDeque[index] = std::move(newPacket);
                mLastUpdate.store(
                    Utilities::getNow<std::chrono::microseconds> (),
                    std::memory_order::seq_cst);
                return; 
            }
            // Insert the element before its upper bounding element
            mDeque.insert(it, std::move(newPacket));
            // Pop first element
            if (isFull){mDeque.pop_front();}
            mLastUpdate.store(
                Utilities::getNow<std::chrono::microseconds> (),
                 std::memory_order::seq_cst);; 
            // Debug code checks this is sorted
#ifndef NDEBUG
            assert(std::is_sorted(mDeque.begin(),
                                  mDeque.end(),
                                  [](const auto &lhs, const auto &rhs)
                                  {
                                     return lhs.startTime < rhs.startTime;
                                  }));
#endif
        }


    }

    uint32_t removeExpiredPackets(const std::chrono::nanoseconds &oldestTime)
    {
        auto nRemoved
            = std::erase_if(mDeque, 
                            [&oldestTime](const auto &item)
                            {
                                return item.endTime < oldestTime;
                            });
        return static_cast<uint32_t> (nRemoved);
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet> getAllPackets() const
    {
        std::vector<UDataPacketCacheServiceAPI::V1::Packet> result;
        auto nPackets = mDeque.size();
        result.reserve(nPackets);
        for (const auto &packet : mDeque)
        {
            result.push_back(packet.packet);
        }
        return result;
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet> 
        getPackets(
           const std::pair<std::chrono::nanoseconds,
                           std::chrono::nanoseconds> &startAndEndTime) const
    {
        const auto startTime = startAndEndTime.first;
        const auto endTime = startAndEndTime.second;
        std::vector<UDataPacketCacheServiceAPI::V1::Packet> result;
        if (mDeque.empty()){return result;}
        struct PacketTime queryPacket;
        queryPacket.startTime = startTime;
        queryPacket.endTime = endTime;
        auto it0 = std::ranges::upper_bound(mDeque, queryPacket,
                                            [](const auto &element,
                                               const auto &query)
                                            {
                                                return query.startTime >= element.startTime;
                                            }); 
        if (it0 == mDeque.end()){return result;}
        // Attempt to move back one b/c of how upper_bound works
        if (it0 != mDeque.begin() && it0->startTime > startTime)
        {
            it0 = std::prev(it0, 1); 
            // If the end time of the previous packet is before t0MuS
            // then restore the iterator as this packet is too old.
            if (it0->endTime < startTime){it0 = std::next(it0);}
        }
        // Unusual but easy
        if (startTime == endTime)
        {
            result.push_back(it0->packet);
            return result;
        }
        // Figure out the end index 
        auto it1 = mDeque.end();
        if (endTime < mDeque.back().startTime)
        {
            it1 = std::ranges::lower_bound(mDeque,
                                           queryPacket,
                                           [](const auto &element, const auto &query) 
                                           {   
                                               return query.endTime >= element.startTime;
                                           });
        }   
        // Just one packet
        if (it0 == it1)
        {
            result.push_back(it0->packet);
            return result;
        }
        // General copy
#ifndef NDEBUG
        // Don't want an infinite copy
        assert(std::distance(mDeque.begin(), it0) <
               std::distance(mDeque.begin(), it1));
#endif
        auto nPackets = static_cast<int> (std::distance(it0, it1));
        if (nPackets < 1){return result;} // This would be weird by this point
        result.reserve(nPackets);
//auto i0 = std::ranges::distance(mDeque.begin(), it0);
//auto i1 = std::ranges::distance(mDeque.begin(), it1);
//std::cout << i0 << " to " << i1 << std::endl;
        for (auto &it = it0; it != it1; std::advance(it, 1)) 
        {
            // Maybe bail early
            result.push_back(it->packet);
            if (endTime <= it->endTime){break;}
        }
#ifndef NDEBUG
        assert(nPackets == static_cast<int> (result.size()));
#endif
        return result;
    }

///private:
    StreamDequeOptions mOptions;
    std::deque<::PacketTime> mDeque;
    std::string mStreamIdentifier;
    size_t mMaximumNumberOfPackets{8192};
    //std::chrono::nanoseconds mMaximumDuration;
    // N.B. creation counts as an update
    std::atomic<std::chrono::microseconds> mLastUpdate;
};

/// Constructor
StreamDeque::StreamDeque(
    const StreamDequeOptions &options,
    UDataPacketServiceAPI::V1::Packet &&packet) :
    pImpl(std::make_unique<StreamDequeImpl> (options))
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Packet identifier not set");
    }
    pImpl->mStreamIdentifier = Utilities::toString(packet.stream_identifier());
    addPacket(std::move(packet)); 
}

StreamDeque::StreamDeque(
    const StreamDequeOptions &options,
    const UDataPacketServiceAPI::V1::Packet &packet) :
    pImpl(std::make_unique<StreamDequeImpl> (options))
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Packet identifier not set");
    }
    pImpl->mStreamIdentifier = Utilities::toString(packet.stream_identifier());
    addPacket(packet);
}


/// Add a packet
void StreamDeque::addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Stream identifier not set");
    }
    if (!packet.has_sampling_rate())
    {
        throw std::invalid_argument("Sampling rate not set");
    }
    if (!packet.has_start_time())
    {
        throw std::invalid_argument("Start time is not set");
    }
    if (packet.number_of_samples() < 1)
    {
        throw std::invalid_argument("No samples");
    }
#ifndef NDEBUG
    auto thisIdentifier = Utilities::toString(packet.stream_identifier());
    if (thisIdentifier != getIdentifier())
    {
        throw std::invalid_argument(
              "This stream identifier ("
            + thisIdentifier
            + ") does not match expected stream identifier ("
            + getIdentifier() + ")");
    }
#endif
    auto convertedPacket = Utilities::convert(std::move(packet));
    //auto copy = convertedPacket;
    pImpl->addPacket(std::move(convertedPacket));
}

void StreamDeque::addPacket(const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    addPacket(std::move(copy));
}

std::vector<UDataPacketCacheServiceAPI::V1::Packet> 
StreamDeque::getPackets(
    const std::pair<std::chrono::nanoseconds,
                   std::chrono::nanoseconds> &startAndEndTime) const
{
    if (startAndEndTime.second < startAndEndTime.first)
    {
        throw std::invalid_argument("Start time exceeds end time");
    }
    return pImpl->getPackets(startAndEndTime);
    //return pImpl->getPackets2(startAndEndTime);
}

std::vector<UDataPacketCacheServiceAPI::V1::Packet>
    StreamDeque::getAllPackets() const
{
    return pImpl->getAllPackets();
//    return pImpl->getAllPackets2();
}

/// Identifier
std::string StreamDeque::getIdentifier() const noexcept
{
    return pImpl->mStreamIdentifier;
}

/// Destructor
StreamDeque::~StreamDeque() = default;

/// Purge the old packets
uint32_t StreamDeque::removeExpiredPackets(
    const std::chrono::nanoseconds &oldestTime)
{
    return pImpl->removeExpiredPackets(oldestTime);
}

/// Last update
std::chrono::microseconds StreamDeque::getLastUpdate() const noexcept
{
    return pImpl->mLastUpdate.load(std::memory_order::seq_cst);
}

///--------------------------------------------------------------------------///
///                            Template Instantiation                        ///
///--------------------------------------------------------------------------///

//template class UDataPacketCacheService::StreamDeque<UDataPacketCacheServiceAPI::V1::Packet>;
