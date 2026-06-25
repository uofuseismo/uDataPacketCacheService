#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>
#include <utility>
#include <google/protobuf/util/time_util.h>
#include <catch2/catch_test_macros.hpp>
//#include <catch2/matchers/catch_matchers.hpp>
//#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <uDataPacketCacheServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
#include "uDataPacketCacheService/streamDeque.hpp"
#include "uDataPacketCacheService/streamDequeOptions.hpp"
#include "uDataPacketCacheService/utilities.hpp"
#include "packUnpack.hpp"

using namespace UDataPacketCacheService;

namespace
{

/// @brief Gets the current time
std::chrono::nanoseconds getNowSimple()
{
     auto now 
        = std::chrono::duration_cast<std::chrono::seconds>
          ((std::chrono::high_resolution_clock::now()).time_since_epoch());
     return now;
}   


UDataPacketServiceAPI::V1::Packet
    createPacket(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
                 const double samplingRate,
                 const std::vector<int> &data)
{
    UDataPacketServiceAPI::V1::Packet result;

    result.set_sampling_rate(samplingRate);
    result.set_data_type(
        UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32);
    result.set_number_of_samples(static_cast<int> (data.size()));
    *result.mutable_data() = ::pack(data);
    *result.mutable_stream_identifier() = identifier;
    return result;
}

std::vector<UDataPacketServiceAPI::V1::Packet>
    createPackets(int nPackets = 10)
{
    UDataPacketServiceAPI::V1::StreamIdentifier identifier; 
    identifier.set_network("UU");
    identifier.set_station("ICU");
    identifier.set_channel("EHZ");
    identifier.set_location_code("01");

    std::vector<UDataPacketServiceAPI::V1::Packet> packets;

    constexpr double samplingRate{100};
    std::uniform_int_distribution<int> packetSizeDistribution(50, 150);
    std::uniform_int_distribution<int> sampleDistribution(-100, 100);
    std::mt19937 generator(28332);
     
    int nTotalSamples{0};
    for (int i = 0; i < nPackets; ++i)
    {
        auto nSamples = packetSizeDistribution(generator); 
        std::vector<int> data(nSamples);
        for (int s = 0; s < nSamples; ++s)
        {
            data[s] = sampleDistribution(generator);
        }
        nTotalSamples = nTotalSamples + nSamples;
        auto packet = ::createPacket(identifier, samplingRate, data);
        packets.push_back(std::move(packet)); 
    } 
    // Fix the start time
    auto now = ::getNowSimple();
    auto dtNanoSeconds
        = static_cast<int64_t> (std::round(1.e9/samplingRate));
    auto durationNanoSeconds
        = static_cast<int64_t> 
          (
             dtNanoSeconds*(nTotalSamples - 1)
          );
    auto startTime = now - std::chrono::nanoseconds {durationNanoSeconds};
    auto nextStartTime = startTime;
    for (auto &packet : packets)
    {
        *packet.mutable_start_time()
            = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
                nextStartTime.count());
        auto endTime
            = Utilities::getEndTime<std::chrono::nanoseconds> (packet);
        nextStartTime = endTime + std::chrono::nanoseconds {dtNanoSeconds};
    }
    std::sort(packets.begin(), packets.end(),
              [](const auto &lhs, const auto &rhs)
              {
                  return Utilities::getStartTime<std::chrono::nanoseconds> (lhs) <
                         Utilities::getEndTime<std::chrono::nanoseconds> (rhs);
              });
    return packets;
}

bool packetMatches(const UDataPacketServiceAPI::V1::Packet &reference,
                   const UDataPacketCacheServiceAPI::V1::Packet &queried)
{
    if (reference.number_of_samples() != queried.number_of_samples())
    {
        return false;
    }
    if (std::abs(reference.sampling_rate() - queried.sampling_rate()) > 1.e-12)
    {
        return false;
    }
    if (Utilities::getStartTime<std::chrono::nanoseconds> (reference) !=
        Utilities::getStartTime<std::chrono::nanoseconds> (queried))
    {
        return false;
    }
    if (reference.data() != queried.data()){return false;}
    return true;
}

bool packetMatches(const UDataPacketCacheServiceAPI::V1::Packet &queried,
                   const UDataPacketServiceAPI::V1::Packet &reference)
{
    return packetMatches(reference, queried);
}

bool packetsMatch(
    const std::vector<UDataPacketCacheServiceAPI::V1::Packet> &queried,
    const std::vector<UDataPacketServiceAPI::V1::Packet> &reference)
{
    if (queried.size() != reference.size()){return false;}
    for (const auto &q : queried)
    {
        bool matched{false};
        for (const auto &r : reference)
        {
            if (packetMatches(r, q))
            {
                matched = true;
                break;
            }
        }
        if (!matched){return false;}
    }
    return true;
}

bool packetsMatch(
    const std::vector<UDataPacketServiceAPI::V1::Packet> &reference,
    const std::vector<UDataPacketCacheServiceAPI::V1::Packet> &queried)
{
    return packetsMatch(queried, reference);
}
 
}

TEST_CASE("UDataPacketCacheService::StreamDeque", "[streamDequeBasic]")
{
    constexpr int maxPackets{10}; // Keep it easy for now
    const int nPackets{10};
    StreamDequeOptions options;
    options.setMaximumNumberOfPackets(maxPackets);

    auto packets = ::createPackets(nPackets);

    SECTION("Simple in order")
    {
        StreamDeque deque{options, packets.at(0)};
        REQUIRE(deque.getIdentifier()
                == Utilities::toString(packets.at(0).stream_identifier()));
        for (int i = 1; i < static_cast<int> (packets.size()); ++i)
        {
            REQUIRE_NOTHROW(deque.addPacket(packets.at(i)));
        }
        auto allPackets = deque.getAllPackets();
        REQUIRE(allPackets.size() == packets.size());
        REQUIRE(std::is_sorted(allPackets.begin(),
                               allPackets.end(),
                               [](const auto &lhs, const auto &rhs)
                               {
                                   return lhs.start_time() < rhs.start_time();
                               }) == true);
        REQUIRE(packetsMatch(packets, allPackets) == true);

        // Query
        constexpr size_t iStart{2};
        constexpr size_t iEnd{7};
        auto startTime = Utilities::getStartTime<std::chrono::nanoseconds>
                         (packets.at(iStart));
        auto endTime = Utilities::getEndTime<std::chrono::nanoseconds>
                       (packets.at(iEnd));
        auto queriedPackets = deque.getPackets(std::pair {startTime, endTime});
        REQUIRE(queriedPackets.size() == iEnd - iStart + 1);
        for (size_t i = iStart; i <= iEnd; ++i)
        {
            REQUIRE(::packetMatches(packets.at(i), 
                                    queriedPackets.at(i - iStart)) == true);
        }
    }

    SECTION("Roll through - standard behavior")
    {
        auto doublePackets = ::createPackets(2*nPackets);
        StreamDequeOptions optionsN;
        optionsN.setMaximumNumberOfPackets(nPackets);
        StreamDeque deque{optionsN, doublePackets.at(0)};
        REQUIRE(deque.getIdentifier()
                == Utilities::toString(doublePackets.at(0).stream_identifier()));
        for (int i = 1; i < static_cast<int> (doublePackets.size()); ++i)
        {
            REQUIRE_NOTHROW(deque.addPacket(doublePackets.at(i)));
        }   
        auto allPackets = deque.getAllPackets();
        REQUIRE(static_cast<int> (allPackets.size()) == nPackets);
        for (int i = nPackets; i < 2*nPackets; ++i)
        {
            REQUIRE(::packetMatches(doublePackets.at(i), 
                                    allPackets.at(i - nPackets)) == true);
        } 
    }

    SECTION("Out of order")
    {
        std::mt19937 generator(29858);
        auto shuffledPackets = packets;
        std::shuffle(shuffledPackets.begin(), shuffledPackets.end(), generator);

        StreamDeque deque{options, shuffledPackets.at(0)};
        REQUIRE(deque.getIdentifier()
                == Utilities::toString(packets.at(0).stream_identifier()));
        for (int i = 1; i < static_cast<int> (shuffledPackets.size()); ++i)
        {
            REQUIRE_NOTHROW(deque.addPacket(shuffledPackets.at(i)));
        }
        auto allPackets = deque.getAllPackets();
        REQUIRE(allPackets.size() == packets.size());
        REQUIRE(std::is_sorted(allPackets.begin(),
                               allPackets.end(),
                               [](const auto &lhs, const auto &rhs)
                               {
                                   return lhs.start_time() < rhs.start_time();
                               }) == true);
        REQUIRE(packetsMatch(packets, allPackets) == true);
    }

    SECTION("Bad timing")
    {
        auto perturbedPackets = packets;
        for (auto &p : perturbedPackets)
        {
            auto startTime
                = Utilities::getStartTime<std::chrono::nanoseconds> (p);
            startTime = startTime + std::chrono::nanoseconds {1000};
            *p.mutable_start_time()
                = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
                  startTime.count());
        }

        StreamDequeOptions extraSpace;
        extraSpace.setMaximumNumberOfPackets(nPackets*2);
        StreamDeque deque{extraSpace, packets.at(0)};
        REQUIRE(deque.getIdentifier()
                == Utilities::toString(packets.at(0).stream_identifier()));
        for (int i = 1; i < static_cast<int> (packets.size()); ++i)
        {
            REQUIRE_NOTHROW(deque.addPacket(packets.at(i)));
        }
        for (const auto &packet : perturbedPackets)
        {
            REQUIRE_NOTHROW(deque.addPacket(packet));
        }
        const auto allPackets = deque.getAllPackets();
    }

    SECTION("Duplicates")
    {
        constexpr size_t iStart{2};
        constexpr size_t iEnd{7};
        auto queryStartTime = Utilities::getStartTime<std::chrono::nanoseconds> (packets[iStart]);
        auto queryEndTime = Utilities::getEndTime<std::chrono::nanoseconds> (packets[iEnd]);

        StreamDequeOptions extraSpace;
        extraSpace.setMaximumNumberOfPackets(nPackets*2);
        StreamDeque deque{extraSpace, packets.at(0)};
        REQUIRE(deque.getIdentifier()
                == Utilities::toString(packets.at(0).stream_identifier()));
        for (int k = 0; k < 2; ++k)
        {
            for (int i = 0; i < static_cast<int> (packets.size()); ++i)
            {
//if (k == 0){std::cout << "send these: " << Utilities::getStartTime<std::chrono::nanoseconds>(packets.at(i)).count()*1.e-9 << std::endl;}
                REQUIRE_NOTHROW(deque.addPacket(packets.at(i)));
            }
        }
        //for (const auto &packet : packets){deque.addPacket(packet);}
        auto allPackets = deque.getAllPackets();
        REQUIRE(allPackets.size() == packets.size());
        REQUIRE(std::is_sorted(allPackets.begin(),
                               allPackets.end(),
                               [](const auto &lhs, const auto &rhs)
                               {
                                   return lhs.start_time() < rhs.start_time();
                               }) == true);
        REQUIRE(packetsMatch(packets, allPackets) == true);

        // Query
        //constexpr size_t iStart{2};
        //constexpr size_t iEnd{7};
        //auto startTime = Utilities::getStartTime<std::chrono::nanoseconds> (packets.at(iStart));
//std::cout << queryStartTime.count() << std::endl;
        auto startTime = queryStartTime + std::chrono::nanoseconds {1000};
        //auto endTime = Utilities::getEndTime<std::chrono::nanoseconds> (packets.at(iEnd));
        auto endTime = queryEndTime - std::chrono::nanoseconds {1000};
//std::cout << startTime.count() << " | " << endTime.count() << std::endl;
//getchar();
        auto queriedPackets = deque.getPackets(std::pair {startTime, endTime});
        REQUIRE(queriedPackets.size() == iEnd - iStart + 1);
        for (size_t i = iStart; i <= iEnd; ++i)
        {
            REQUIRE(::packetMatches(packets.at(i),
                                    queriedPackets.at(i - iStart)) == true);
        }
    }
}
