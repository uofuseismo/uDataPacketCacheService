#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <google/protobuf/util/time_util.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
#include <uDataPacketCacheServiceAPI/v1/packet.pb.h>
#include <uDataPacketCacheServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketCacheServiceAPI/v1/data_type.pb.h>
#include "uDataPacketCacheService/utilities.hpp"

/*
/// @brief Gets the start time
template<>
std::chrono::microseconds
UDataPacketCacheService::Utilities::getStartTime(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
             packet.start_time());
    return std::chrono::microseconds {startTime};
}

template<>
std::chrono::nanoseconds
UDataPacketCacheService::Utilities::getStartTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
             packet.start_time());
    return std::chrono::nanoseconds {startTime};
}
*/

namespace
{

template<typename U>
std::chrono::microseconds getStartTimeMicroSeconds(const U &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
             packet.start_time());
    return std::chrono::microseconds {startTime};
}

template<typename U>
std::chrono::nanoseconds getStartTimeNanoSeconds(const U &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
             packet.start_time());
    return std::chrono::nanoseconds {startTime};
}

template<typename U>
std::chrono::microseconds getEndTimeMicroSeconds(const U &packet)
{   
    const auto nSamples = packet.number_of_samples();
    if (nSamples < 1){throw std::invalid_argument("No samples");}
    const auto samplingRate = packet.sampling_rate();
    if (samplingRate <= 0){throw std::invalid_argument("Sampling rate not positive");}
    constexpr double SECONDS_TO_NANOSECONDS{1000000000};
    const double samplingPeriodNanoSeconds
        = SECONDS_TO_NANOSECONDS/samplingRate;
    const auto iEndTimeNanoSeconds
        = static_cast<int64_t> (
            std::round( (nSamples - 1)*samplingPeriodNanoSeconds ) );
    const std::chrono::microseconds endTime
        = getStartTimeMicroSeconds (packet)
        + std::chrono::duration_cast<std::chrono::microseconds>
            (std::chrono::nanoseconds {iEndTimeNanoSeconds});
    return endTime;
}

template<typename U>
std::chrono::nanoseconds getEndTimeNanoSeconds(const U &packet)
{
    const auto nSamples = packet.number_of_samples();
    if (nSamples < 1){throw std::invalid_argument("No samples");}
    const auto samplingRate = packet.sampling_rate();
    if (samplingRate <= 0){throw std::invalid_argument("Sampling rate not positive");}
    constexpr double SECONDS_TO_NANOSECONDS{1000000000};
    const double samplingPeriodNanoSeconds
        = SECONDS_TO_NANOSECONDS/samplingRate;
    const auto iEndTimeNanoSeconds
        = static_cast<int64_t> (
            std::round( (nSamples - 1)*samplingPeriodNanoSeconds ) );
    const std::chrono::nanoseconds endTime
        = getStartTimeNanoSeconds (packet)
        + std::chrono::nanoseconds {iEndTimeNanoSeconds};
    return endTime;
}

}

// Start time
template<>
std::chrono::microseconds
UDataPacketCacheService::Utilities::getStartTime<std::chrono::microseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    return ::getStartTimeMicroSeconds(packet);
}

template<>
std::chrono::microseconds
UDataPacketCacheService::Utilities::getStartTime<std::chrono::microseconds>(
    const UDataPacketCacheServiceAPI::V1::Packet &packet)
{
    return ::getStartTimeMicroSeconds(packet);
}

template<>
std::chrono::nanoseconds
UDataPacketCacheService::Utilities::getStartTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    return ::getStartTimeNanoSeconds(packet);
}

template<>
std::chrono::nanoseconds
UDataPacketCacheService::Utilities::getStartTime<std::chrono::nanoseconds>(
    const UDataPacketCacheServiceAPI::V1::Packet &packet)
{
    return ::getStartTimeNanoSeconds(packet);
}

// End time
template<>
std::chrono::microseconds 
UDataPacketCacheService::Utilities::getEndTime<std::chrono::microseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    return ::getEndTimeMicroSeconds(packet);
}

template<>
std::chrono::microseconds
UDataPacketCacheService::Utilities::getEndTime<std::chrono::microseconds>(
    const UDataPacketCacheServiceAPI::V1::Packet &packet)
{
    return ::getEndTimeMicroSeconds(packet);
}

template<>
std::chrono::nanoseconds
UDataPacketCacheService::Utilities::getEndTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    return ::getEndTimeNanoSeconds(packet);
}

template<>
std::chrono::nanoseconds
UDataPacketCacheService::Utilities::getEndTime<std::chrono::nanoseconds>(
    const UDataPacketCacheServiceAPI::V1::Packet &packet)
{
    return ::getEndTimeNanoSeconds(packet);
}

/// @brief Gets the current time
template<>
std::chrono::microseconds UDataPacketCacheService::Utilities::getNow()
{
    auto now
       = std::chrono::duration_cast<std::chrono::microseconds>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}   

template<>
std::chrono::nanoseconds UDataPacketCacheService::Utilities::getNow()
{
    auto now 
       = std::chrono::duration_cast<std::chrono::microseconds>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}   

/// @brief Converts the input packet to a string name.
namespace
{
template<typename T>
std::string toString(const T &identifier)
{
    std::string result;
    if (identifier.has_network())
    {
        if (identifier.network().empty())
        {
            throw std::invalid_argument("Network is empty");
        }   
        result = identifier.network();
        result.append(".");
    }
    else
    {
        throw std::invalid_argument("Network not set");
    }

    if (identifier.has_station())
    {
        if (identifier.station().empty())
        {
            throw std::invalid_argument("Station is empty");
        }   
        result.append(identifier.station());
        result.append(".");
    }   
    else
    {   
        throw std::invalid_argument("Station not set");
    }   

    if (identifier.has_channel())
    {
        if (identifier.channel().empty())
        {
            throw std::invalid_argument("Channel is empty");
        }
        result.append(identifier.channel());
        result.append(".");
    }   
    else
    {   
        throw std::invalid_argument("Channel not set");
    }   

    if (identifier.has_location_code())
    {
        if (!identifier.location_code().empty())
        {
            result.append(identifier.location_code());
        }
        else
        {
            result.append("--");
        }
    }   
    else
    {
        result.append("--");
    }   
    result.erase(
        std::remove_if(result.begin(), result.end(), ::isspace),
        result.end());
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

template<typename T>
void fromString(const std::string &identifier, T &result)
{
    std::vector<std::string> splitString;
    boost::algorithm::split(splitString, identifier, boost::is_any_of("."));
    if (splitString.size() == 3 || splitString.size() == 4)
    {
        result.set_network(splitString[0]);
        result.set_station(splitString[1]);
        result.set_channel(splitString[2]);
        if (splitString.size() == 4)
        {
            result.set_location_code(splitString[3]);
        }
        else
        {
            result.set_location_code("--");
        }
    }
    else
    {
        throw std::invalid_argument("Unhandled split string size");
    }
}

}

template<>
UDataPacketServiceAPI::V1::StreamIdentifier 
    UDataPacketCacheService::Utilities::fromString(const std::string &identifier)
{
    UDataPacketServiceAPI::V1::StreamIdentifier result;
    ::fromString(identifier, result);
    return result;    
}

template<>
UDataPacketCacheServiceAPI::V1::StreamIdentifier 
    UDataPacketCacheService::Utilities::fromString(const std::string &identifier)
{
    UDataPacketCacheServiceAPI::V1::StreamIdentifier result;
    ::fromString(identifier, result);
    return result;    
}

std::string UDataPacketCacheService::Utilities::toString(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
    return ::toString(identifier);
}

std::string UDataPacketCacheService::Utilities::toString(
    const UDataPacketCacheServiceAPI::V1::StreamIdentifier &identifier)
{
    return ::toString(identifier);
}

/// @brief Converts stream identifiers
UDataPacketCacheServiceAPI::V1::StreamIdentifier 
    UDataPacketCacheService::Utilities::convert(
        const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
    UDataPacketCacheServiceAPI::V1::StreamIdentifier result;
    if (identifier.has_network())
    {   
        auto work = identifier.network();
        work.erase(
            std::remove_if(work.begin(), work.end(), ::isspace),
            work.end());
        if (work.empty()){throw std::invalid_argument("Network is blank");}
        std::transform(work.begin(), work.end(), work.begin(), ::toupper);
        *result.mutable_network() = std::move(work);
    }   
    else
    {   
        throw std::invalid_argument("Network not set");
    }   

    if (identifier.has_station())
    {   
        auto work = identifier.station();
        work.erase(
            std::remove_if(work.begin(), work.end(), ::isspace),
            work.end());
        if (work.empty()){throw std::invalid_argument("Station is blank");}
        std::transform(work.begin(), work.end(), work.begin(), ::toupper);
        *result.mutable_station() = std::move(work);
    }   
    else
    {   
        throw std::invalid_argument("Station not set");
    }   

    if (identifier.has_channel())
    {
        auto work = identifier.channel();
        work.erase(
            std::remove_if(work.begin(), work.end(), ::isspace),
            work.end());
        if (work.empty()){throw std::invalid_argument("Channel is blank");}
        std::transform(work.begin(), work.end(), work.begin(), ::toupper);
        *result.mutable_channel() = std::move(work);
    }
    else
    {
        throw std::invalid_argument("Channel not set");
    }

    if (identifier.has_location_code())
    {
        auto work = identifier.location_code();
        work.erase(
            std::remove_if(work.begin(), work.end(), ::isspace),
            work.end());
        if (!work.empty())
        {
            std::transform(work.begin(), work.end(), work.begin(), ::toupper);
            *result.mutable_location_code() = std::move(work);
        }
        else
        {
            *result.mutable_location_code() = "--";
        }
    }
    else
    {
        *result.mutable_location_code() = "--";
    }
 
    return result;
}

/// @brief Converts packets
UDataPacketCacheServiceAPI::V1::Packet
    UDataPacketCacheService::Utilities::convert(
        UDataPacketServiceAPI::V1::Packet &&packet)
{
    UDataPacketCacheServiceAPI::V1::Packet result;
    if (!packet.has_sampling_rate())
    {
        throw std::invalid_argument("Sampling rate not set");
    }
    const auto samplingRate = packet.sampling_rate();
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Sampling rate must be positive");
    }
    result.set_sampling_rate(samplingRate);

    if (!packet.has_start_time())
    {
        throw std::invalid_argument("Start time not set");
    }
    *result.mutable_start_time() = std::move(*packet.mutable_start_time()); 

    if (!packet.has_number_of_samples())
    {
        throw std::invalid_argument("Number of samples not set");
    }
    auto nSamples = packet.number_of_samples();
    if (nSamples < 0)
    {
        throw std::invalid_argument("Number of samples must be non-negative");
    }
    result.set_number_of_samples(nSamples);

    namespace UDPS = UDataPacketServiceAPI::V1;
    namespace UDCPS = UDataPacketCacheServiceAPI::V1;
    const auto dataType = packet.data_type();
    if (dataType == UDPS::DataType::DATA_TYPE_INTEGER_32)
    {
        result.set_data_type(UDCPS::DataType::DATA_TYPE_INTEGER_32);
    }
    else if (dataType == UDPS::DataType::DATA_TYPE_DOUBLE)
    {
        result.set_data_type(UDCPS::DataType::DATA_TYPE_DOUBLE);
    }
    else if (dataType == UDPS::DataType::DATA_TYPE_FLOAT)
    {
        result.set_data_type(UDCPS::DataType::DATA_TYPE_FLOAT);
    }
    else if (dataType == UDPS::DataType::DATA_TYPE_INTEGER_64)
    {
        result.set_data_type(UDCPS::DataType::DATA_TYPE_INTEGER_64);
    }
    else if (dataType == UDPS::DataType::DATA_TYPE_TEXT)
    {
        throw std::invalid_argument("Text type not supported");
    }
    else
    {
        if (dataType != UDPS::DataType::DATA_TYPE_UNKNOWN)
        {
            throw std::invalid_argument("Unhandled data type");
        }
        else
        {
            result.set_data_type(UDCPS::DataType::DATA_TYPE_UNKNOWN);
        }
    }

    *result.mutable_data() = std::move(*packet.mutable_data());

    return result;
}

UDataPacketCacheServiceAPI::V1::Packet
    UDataPacketCacheService::Utilities::convert(
        const UDataPacketServiceAPI::V1::Packet &packet)
{
    auto copy = packet;
    return convert(std::move(copy));
}

