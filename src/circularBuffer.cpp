#include <iomanip>
#include <iostream>
#include <algorithm>
#ifndef NDEBUG
#include <cassert>
#endif
#include <chrono>
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
#include <boost/circular_buffer.hpp> //NOLINT
#include "uDataPacketCacheService/circularBuffer.hpp"
#include "uDataPacketCacheService/circularBufferOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"

using namespace UDataPacketCacheService;

namespace
{

struct CircularBufferPacket
{
    UDataPacketCacheServiceAPI::V1::Packet packet;
    std::chrono::nanoseconds startTime;
    std::chrono::nanoseconds endTime;
};

}

class CircularBuffer::CircularBufferImpl
{
public:
    explicit CircularBufferImpl(const CircularBufferOptions &options) :
        mOptions(options)
    {
        auto maxPackets = mOptions.getMaximumNumberOfPackets();
        mCircularBuffer.set_capacity(maxPackets);
    }

    void addPacket(UDataPacketCacheServiceAPI::V1::Packet &&packet)
    {
        // Easier to work with than .first and and .second
        const auto startTime
            = Utilities::getStartTime<std::chrono::nanoseconds> (packet); 
        const auto endTime
            = Utilities::getEndTime<std::chrono::nanoseconds> (packet);

        // Get ready to insert it
        CircularBufferPacket newPacket
        {
             std::move(packet),
             startTime,
             endTime
        };

        // Empty buffer - easy!
        if (mCircularBuffer.empty())
        {
            mCircularBuffer.push_back(std::move(newPacket));
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> (); 
            return;
        }

        // Almost always, we will be inserting at the end
        const auto currentOldestTime = mCircularBuffer.back().endTime;
        if (startTime >= currentOldestTime)
        {
            mCircularBuffer.push_back(std::move(newPacket));
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            return;
        }
        // There's a wonky case where this can happen.  The latest
        // packet's timing slipped just a bit.  It's still the most refresh
        if (startTime > mCircularBuffer.back().startTime)
        {
            mCircularBuffer.push_back(std::move(newPacket));
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            return;
        }

        // Maybe we can insert at the beginning
        const auto currentEarliestTime
            = (mCircularBuffer.front().startTime);
        // For the same reason as below - we assume this data is already
        // acquired and skip it (duplicate or less likely a GPS slip).
        if (startTime == currentEarliestTime)
        {
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            return;
        }
        // The buffer is full and we need to prioritize new data so skip this.
        // Still, we can mark it as `updated' to demonstrate this stream is
        // still active.
        if (startTime < currentEarliestTime && mCircularBuffer.full())
        {
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            return;
        }
        // Okay, there's space at the beginning so accomodate it.
        // N.B. it must also be true that startTime < currentEarliestTime
        if (endTime <= currentEarliestTime && !mCircularBuffer.full())
        {
            mCircularBuffer.push_front(std::move(newPacket));
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            return;
        }

        auto it
            = std::ranges::upper_bound(mCircularBuffer,
                                       newPacket,
                                       [](const auto &element, const auto &query)
              {
                  return element.startTime <= query.startTime;
              });
        if (it != mCircularBuffer.end())
        {
            // We have an exact match.  Do nothing because it is either
            // a backfill (same thing) or it's a timing error which in 
            // the near-real time context more likely indicates a problem.
            //auto index = std::distance(mCircularBuffer.begin(), it);
            const auto startTimeNeighbor = it->startTime; //mCircularBuffer[index].startTime;
            if (startTime == startTimeNeighbor)
            {
                //mCircularBuffer[index] = std::move(newPacket);
                mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
                return; 
            } 
            // Insert the element before its upper bounding element
            mCircularBuffer.rinsert(it, std::move(newPacket));
            mLastUpdate = Utilities::getNow<std::chrono::microseconds> ();
            // Debug code checks this is sorted
#ifndef NDEBUG
            assert(std::is_sorted(mCircularBuffer.begin(),
                                  mCircularBuffer.end(),
                                  [](const auto &lhs, const auto &rhs)
                                  {
                                     return lhs.startTime < rhs.startTime;
                                  }));
#endif
        }
    }

    void removeExpiredPackets(const std::chrono::nanoseconds &oldestTime)
    {
        mCircularBuffer.erase(
            std::remove_if(mCircularBuffer.begin(), mCircularBuffer.end(), 
                           [&oldestTime](const auto &item)
                           {
                               return item.endTime < oldestTime;
                           })
        );
    }

    [[nodiscard]] std::vector<UDataPacketCacheServiceAPI::V1::Packet> getAllPackets() const
    {
        std::vector<UDataPacketCacheServiceAPI::V1::Packet> result;
        auto nPackets = mCircularBuffer.size();
        result.reserve(nPackets);
        for (const auto &packet : mCircularBuffer)
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
        if (mCircularBuffer.empty()){return result;}
        struct CircularBufferPacket queryPacket;
        queryPacket.startTime = startTime;
        queryPacket.endTime = endTime;
        auto it0 = std::ranges::upper_bound(mCircularBuffer, queryPacket,
                                            [](const auto &element,
                                               const auto &query)
                                            {
                                                return query.startTime >= element.startTime;
                                            });
//std::cout << std::setprecision(16) << startTime.count()*1.e-9 << " " << endTime.count()*1.e-9 << std::endl;
//int i = 0;
//for (auto &item : mCircularBuffer)
//{
 //std::cout << std::setprecision(16) << i << " " << item.startTime.count()*1.e-9 << "," << item.endTime.count()*1.e-9 << std::endl;
//++i;
//}
/*
        auto it0
            = std::upper_bound(mCircularBuffer.begin(),
                               mCircularBuffer.end(), 
                               startTime,
                               [](const std::chrono::nanoseconds queryStartTime,
                                  const auto &rhs)
                               {
                                   return queryStartTime >= rhs.startTime;
                               });
*/
        if (it0 == mCircularBuffer.end()){return result;}
        // Attempt to move back one b/c of how upper_bound works
        if (it0 != mCircularBuffer.begin() && it0->startTime > startTime)
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
        auto it1 = mCircularBuffer.end();
        if (endTime < mCircularBuffer.back().startTime)
        {
            it1 = std::ranges::lower_bound(mCircularBuffer,
                                           queryPacket,
                                           [](const auto &element, const auto &query) 
                                           {
                                               return query.endTime >= element.startTime;
                                           });
            /*
            it1
                = std::upper_bound(
                    mCircularBuffer.begin(),
                    mCircularBuffer.end(), 
                    endTime,
                    [](const std::chrono::nanoseconds queryEndTime,
                       const auto &rhs)
                    {
                        return queryEndTime >= rhs.startTime;
                    });
            */
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
        assert(std::distance(mCircularBuffer.begin(), it0) <
               std::distance(mCircularBuffer.begin(), it1));
#endif
        auto nPackets = static_cast<int> (std::distance(it0, it1));
        if (nPackets < 1){return result;} // This would be weird by this point
        result.reserve(nPackets);
//auto i0 = std::ranges::distance(mCircularBuffer.begin(), it0);
//auto i1 = std::ranges::distance(mCircularBuffer.begin(), it1);
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
    CircularBufferOptions mOptions;
    // NOLINTNEXTLINE(misc-include-cleaner)
    //boost::circular_buffer_space_optimized<::CircularBufferPacket> mCircularBuffer;
    // NOLINTNEXTLINE(misc-include-cleaner)
    boost::circular_buffer<::CircularBufferPacket> mCircularBuffer;
    std::string mStreamIdentifier;
    //std::chrono::nanoseconds mMaximumDuration;
    std::chrono::microseconds mLastUpdate{0};
};

/// Constructor
CircularBuffer::CircularBuffer(
    const CircularBufferOptions &options,
    UDataPacketServiceAPI::V1::Packet &&packet) :
    pImpl(std::make_unique<CircularBufferImpl> (options))
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Packet identifier not set");
    }
    pImpl->mStreamIdentifier = Utilities::toString(packet.stream_identifier());
    addPacket(std::move(packet)); 
}

CircularBuffer::CircularBuffer(
    const CircularBufferOptions &options,
    const UDataPacketServiceAPI::V1::Packet &packet) :
    pImpl(std::make_unique<CircularBufferImpl> (options))
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Packet identifier not set");
    }
    pImpl->mStreamIdentifier = Utilities::toString(packet.stream_identifier());
    addPacket(packet);
}


/// Add a packet
void CircularBuffer::addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
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
    pImpl->addPacket(std::move(convertedPacket));
}

void CircularBuffer::addPacket(const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    addPacket(std::move(copy));
}

std::vector<UDataPacketCacheServiceAPI::V1::Packet> 
CircularBuffer::getPackets(
    const std::pair<std::chrono::nanoseconds,
                   std::chrono::nanoseconds> &startAndEndTime) const
{
    if (startAndEndTime.second < startAndEndTime.first)
    {
        throw std::invalid_argument("Start time exceeds end time");
    }
    return pImpl->getPackets(startAndEndTime);
}

std::vector<UDataPacketCacheServiceAPI::V1::Packet>
    CircularBuffer::getAllPackets() const
{
    return pImpl->getAllPackets();
}

/// Identifier
std::string CircularBuffer::getIdentifier() const noexcept
{
    return pImpl->mStreamIdentifier;
}

/// Destructor
CircularBuffer::~CircularBuffer() = default;

/// Purge the old packets
void CircularBuffer::removeExpiredPackets(
    const std::chrono::nanoseconds &oldestTime)
{
    pImpl->removeExpiredPackets(oldestTime);
}

///--------------------------------------------------------------------------///
///                            Template Instantiation                        ///
///--------------------------------------------------------------------------///

//template class UDataPacketCacheService::CircularBuffer<UDataPacketCacheServiceAPI::V1::Packet>;
