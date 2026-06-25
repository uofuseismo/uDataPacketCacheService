#ifndef UDATA_PACKET_CACHE_SERVICE_UTILITIES_HPP
#define UDATA_PACKET_CACHE_SERVICE_UTILITIES_HPP
#include <chrono>
#include <string>
namespace UDataPacketServiceAPI::V1
{
 class StreamIdentifier;
 class Packet;
}
namespace UDataPacketCacheServiceAPI::V1
{
 class StreamIdentifier;
 class Packet;
} 

namespace UDataPacketCacheService::Utilities
{

/// @brief Converts stream identifiers.
UDataPacketCacheServiceAPI::V1::StreamIdentifier
    convert(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier);
/// @brief Converts packets.
UDataPacketCacheServiceAPI::V1::Packet
    convert(UDataPacketServiceAPI::V1::Packet &&packet);
UDataPacketCacheServiceAPI::V1::Packet
    convert(const UDataPacketServiceAPI::V1::Packet &packet);
/// @result The packet start time (microseconds or nanoseconds).
template<typename T>
T getStartTime(const UDataPacketServiceAPI::V1::Packet &packet);
template<typename T>
T getStartTime(const UDataPacketCacheServiceAPI::V1::Packet &packet);
/// @result The packet end time (microseconds or nanoseconds).
template<typename T>
T getEndTime(const UDataPacketServiceAPI::V1::Packet &packet);
template<typename T>
T getEndTime(const UDataPacketCacheServiceAPI::V1::Packet &packet);
/// @result The current time in microseconds or nanoseconds.
template<typename T> T getNow();
/// @brief Converts the stream identifier to a string representation.
[[nodiscard]] std::string toString(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier);
[[nodiscard]] std::string toString(const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier);
/// @brief Inverse function of toString.
template<typename T> T fromString(const std::string &identifier);
/// @brief Utility to check if an import packet is valid.
[[nodiscard]] bool isValid(const UDataPacketServiceAPI::V1::Packet &packet,
                           std::string &reason);
}
#endif
