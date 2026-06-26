#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include <google/protobuf/util/time_util.h>
#include <uDataPacketCacheServiceAPI/v1/packet.pb.h>
#include <uDataPacketCacheServiceAPI/v1/data_type.pb.h>
#include <uDataPacketCacheServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketCacheServiceAPI/v1/stream_request.pb.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
//#include <catch2/catch_approx.hpp>
//#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketCacheService/utilities.hpp"
#include "packUnpack.hpp"

using namespace UDataPacketCacheService;

namespace
{

/*
template<typename T>
std::vector<T>
    unpack(const std::string &data, const uint32_t nSamples)
{
    const bool swapBytes
        = (std::endian::native == std::endian::little) ? false : true;
    constexpr auto dataTypeSize = sizeof(T);
    std::vector<T> result;
    if (nSamples < 1){return result;}
    if (static_cast<size_t> (nSamples)*dataTypeSize != data.size())
    {   
        throw std::invalid_argument("Unexpected data size");
    }   
    result.resize(nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        unsigned char cArray[dataTypeSize];
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {   
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::copy(data.data() + i1, data.data() + i2, 
                      cvUnion.cArray);
            result[i] = cvUnion.value;
        }
    }
    else
    {   
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::reverse_copy(data.data() + i1, data.data() + i2, 
                              cvUnion.cArray);
            result[i] = cvUnion.value;
        }
    }
    return result;
}

template<typename T>
std::string pack(const T *data, const int nSamples, const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::string result;
    if (nSamples < 1){return result;}
    result.resize(dataTypeSize*nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        char cArray[dataTypeSize]; // Unpack uses unsigned char so this pushes it
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                      result.data() + dataTypeSize*i);
        }
    }   
    else
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::reverse_copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                              result.data() + dataTypeSize*i);
        }
    }   
    return result;
}

template<typename T>
std::string pack(const std::vector<T> &data)
{
    const bool swapBytes
    {   
        std::endian::native == std::endian::little ? false : true
    };  
    return ::pack(data.data(), static_cast<int> (data.size()), swapBytes);
}
*/

template<typename T>
UDataPacketServiceAPI::V1::DataType toDataType()
{
    if (std::is_same<T, int>::value || std::is_same<T, int32_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32;
    }   
    else if (std::is_same<T, int64_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_64;
    }   
    else if (std::is_same<T, float>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_FLOAT;
    }   
    else if (std::is_same<T, double>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_DOUBLE;
    }   
    else if (std::is_same<T, char>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_TEXT;
    }   
    return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN;
}

template<typename T>
UDataPacketCacheServiceAPI::V1::DataType toOutputDataType()
{
    if (std::is_same<T, int>::value || std::is_same<T, int32_t>::value)
    {   
        return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32;
    }   
    else if (std::is_same<T, int64_t>::value)
    {   
        return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_INTEGER_64;
    }   
    else if (std::is_same<T, float>::value)
    {   
        return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_FLOAT;
    }   
    else if (std::is_same<T, double>::value)
    {   
        return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_DOUBLE;
    }   
/*
    {   
        return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_TEXT;
    }   
*/
    return UDataPacketCacheServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN;
}


}

TEST_CASE("UDataPacketCacheService::Utilities", "[toString::input]")
{
    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    // NOLINTBEGIN
    REQUIRE(Utilities::toString(identifier) == "UU.HVU.HHZ.01");
    // NOLINTEND
    const auto inverse
        = Utilities::fromString<UDataPacketServiceAPI::V1::StreamIdentifier>
          (Utilities::toString(identifier));
    REQUIRE(inverse.network() == "UU");
    REQUIRE(inverse.station() == "HVU");
    REQUIRE(inverse.channel() == "HHZ");
    REQUIRE(inverse.location_code() == "01");
}

TEST_CASE("UDataPacketCacheService::Utilities", "[toString::output]")
{
    const std::string network{"Uu"};
    const std::string station{"hVU"};
    const std::string channel{"H hZ"};
    const std::string locationCode{""};

    UDataPacketCacheServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    // NOLINTBEGIN
    REQUIRE(Utilities::toString(identifier) == "UU.HVU.HHZ.--");
    // NOLINTEND
    const auto inverse
         = Utilities::fromString<UDataPacketCacheServiceAPI::V1::StreamIdentifier>
           ("UU.HVU.HHZ");
    REQUIRE(inverse.network() == "UU");
    REQUIRE(inverse.station() == "HVU");
    REQUIRE(inverse.channel() == "HHZ");
    REQUIRE(inverse.location_code() == "--");
}

TEST_CASE("UDataPacketCacheService::Utilities", "[convertIdentifier]")
{
    SECTION("With Location Code")
    {
        const std::string network{"Uu"};
        const std::string station{" hVU"};
        const std::string channel{"HH Z"};
        const std::string locationCode{"01"};

        UDataPacketServiceAPI::V1::StreamIdentifier identifier;
        identifier.set_network(network);
        identifier.set_station(station);
        identifier.set_channel(channel);
        identifier.set_location_code(locationCode);

        auto result = Utilities::convert(identifier);
        REQUIRE(result.network() == "UU");
        REQUIRE(result.station() == "HVU");
        REQUIRE(result.channel() == "HHZ");
        REQUIRE(result.location_code() == "01");
    }

    SECTION("Without Location Code")
    {
        const std::string network{" Uu"};
        const std::string station{"hVU"};
        const std::string channel{" hH Z"};

        UDataPacketServiceAPI::V1::StreamIdentifier identifier;
        identifier.set_network(network);
        identifier.set_station(station);
        identifier.set_channel(channel);

        auto result = Utilities::convert(identifier);
        REQUIRE(result.network() == "UU");
        REQUIRE(result.station() == "HVU");
        REQUIRE(result.channel() == "HHZ");
        REQUIRE(result.location_code() == "--");
    }
}

TEST_CASE("UDataPacketCacheService::Utilities", "[isValid]")
{
    const std::vector<int> data{-1, -0, 1, 2, 3, 4, 5, 6, 7, 8}; 
    const std::chrono::seconds startTimeS{1774627730};
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    constexpr double samplingRate{100};
    const int nSamples = static_cast<int> (data.size());

    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network("UU");
    identifier.set_station("EKU");
    identifier.set_channel("EHZ");
    identifier.set_location_code("01");

    UDataPacketServiceAPI::V1::Packet packet;
    std::string reason;

    REQUIRE(Utilities::isValid(packet, reason) == false);
    *packet.mutable_stream_identifier() = identifier;
    REQUIRE(Utilities::isValid(packet, reason) == false);
    packet.set_sampling_rate(samplingRate);
    REQUIRE(Utilities::isValid(packet, reason) == false);
    *packet.mutable_start_time() = startTime;
    REQUIRE(Utilities::isValid(packet, reason) == false);
    packet.set_data_type(::toDataType<int>());
    REQUIRE(Utilities::isValid(packet, reason) == false);
    packet.set_number_of_samples(nSamples);
    REQUIRE(Utilities::isValid(packet, reason) == false);
    packet.set_data(::pack(data));
    REQUIRE(Utilities::isValid(packet, reason) == true);
}

TEST_CASE("UDataPacketCacheService::Utilities", "[isValidStreamRequest]")
{
    std::string reason;
    const std::chrono::seconds startTimeS{1774627730};
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    const auto endTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count() + 1);
    const std::chrono::seconds badEndTimeS{1774627729};
    const auto badEndTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            badEndTimeS.count());

    UDataPacketCacheServiceAPI::V1::StreamRequest request;
    REQUIRE(Utilities::isValid(request, reason) == false);

    UDataPacketCacheServiceAPI::V1::StreamIdentifier identifier;

    identifier.set_network("UU");
    *request.mutable_stream_identifier() = identifier;
    REQUIRE(Utilities::isValid(request, reason) == false);
    identifier.set_station("EKU");
    *request.mutable_stream_identifier() = identifier;
    REQUIRE(Utilities::isValid(request, reason) == false);
    identifier.set_channel("EHZ");
    *request.mutable_stream_identifier() = identifier;
    REQUIRE(Utilities::isValid(request, reason) == false);
    identifier.set_location_code("01");
    *request.mutable_stream_identifier() = identifier;
    REQUIRE(Utilities::isValid(request, reason) == false);

    *request.mutable_start_time() = startTime;
    REQUIRE(Utilities::isValid(request, reason) == false);
    *request.mutable_end_time() = badEndTime;
    REQUIRE(Utilities::isValid(request, reason) == false); 
    *request.mutable_end_time() = endTime;
    REQUIRE(Utilities::isValid(request, reason) == true);
}

TEST_CASE("UDataPacketCacheService::Utiliites", "[startEndTime]")
{
    const std::vector<int> data{-1, -0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::chrono::seconds startTimeS{1774627730};
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    constexpr double samplingRate{100};
    const int nSamples = static_cast<int> (data.size());
    SECTION("Import")
    {
        UDataPacketServiceAPI::V1::Packet packet;
        //*packet.mutable_stream_identifier() = identifier;
        *packet.mutable_start_time() = startTime;
        packet.set_sampling_rate(samplingRate);
        packet.set_number_of_samples(nSamples);
        packet.set_data_type(::toDataType<int>());
        packet.set_data(::pack(data));

        REQUIRE(Utilities::getStartTime<std::chrono::microseconds> (packet) ==
                std::chrono::microseconds {startTimeS});
        REQUIRE(Utilities::getStartTime<std::chrono::nanoseconds> (packet) ==
                std::chrono::nanoseconds {startTimeS});
        const int64_t endTimeMuS
            = std::chrono::microseconds{startTimeS}.count()
            + static_cast<int64_t> (std::round((nSamples - 1)/samplingRate*1.e6));
        const auto endTimeNanoS
            = std::chrono::nanoseconds{startTimeS}.count()
            + static_cast<int64_t> (std::round((nSamples - 1)/samplingRate*1.e9));
        REQUIRE(Utilities::getEndTime<std::chrono::microseconds> (packet) ==
                std::chrono::microseconds {endTimeMuS});
        REQUIRE(Utilities::getEndTime<std::chrono::nanoseconds> (packet) ==
                std::chrono::nanoseconds {endTimeNanoS});
    }   

    SECTION("Export")
    {
        UDataPacketCacheServiceAPI::V1::Packet packet;
        //*packet.mutable_stream_identifier() = identifier;
        *packet.mutable_start_time() = startTime;
        packet.set_sampling_rate(samplingRate);
        packet.set_number_of_samples(nSamples);
        packet.set_data_type(::toOutputDataType<int>());
        packet.set_data(::pack(data));

        REQUIRE(Utilities::getStartTime<std::chrono::microseconds> (packet) ==
                std::chrono::microseconds {startTimeS});
        REQUIRE(Utilities::getStartTime<std::chrono::nanoseconds> (packet) ==
                std::chrono::nanoseconds {startTimeS});
        const int64_t endTimeMuS 
            = std::chrono::microseconds{startTimeS}.count()
            + static_cast<int64_t> (std::round((nSamples - 1)/samplingRate*1.e6));
        const auto endTimeNanoS
            = std::chrono::nanoseconds{startTimeS}.count()
            + static_cast<int64_t> (std::round((nSamples - 1)/samplingRate*1.e9));
        REQUIRE(Utilities::getEndTime<std::chrono::microseconds> (packet) ==
                std::chrono::microseconds {endTimeMuS});
        REQUIRE(Utilities::getEndTime<std::chrono::nanoseconds> (packet) ==
                std::chrono::nanoseconds {endTimeNanoS});
    }   
}

TEMPLATE_TEST_CASE("UDataPacketCacheService::Utilities", "[convertPacket]",
                   int, float, double, int64_t)
{
    const std::vector<TestType> data{-1, -0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::chrono::seconds startTimeS{1774627730};
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    const double samplingRate{100};
    const auto nSamples = static_cast<int> (data.size());
    
    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    UDataPacketServiceAPI::V1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    *packet.mutable_start_time() = startTime;
    packet.set_sampling_rate(samplingRate);
    packet.set_number_of_samples(nSamples);
    packet.set_data_type(::toDataType<TestType>());
    packet.set_data(::pack(data));

    auto result = Utilities::convert(packet);
    REQUIRE(
       google::protobuf::util::TimeUtil::TimestampToSeconds(result.start_time())
       == startTimeS.count()); 
    REQUIRE_THAT(result.sampling_rate(),
            Catch::Matchers::WithinRel(
               samplingRate,
               std::numeric_limits<double>::epsilon()*100));
    REQUIRE(result.number_of_samples() == static_cast<int> (data.size()));
    if (std::is_same<TestType, int>::value ||
        std::is_same<TestType, int32_t>::value)
    {
        auto nBackSamples = result.number_of_samples();
        auto dataBack = ::unpack<TestType>(result.data(), nBackSamples);
        REQUIRE(result.data_type() == ::toOutputDataType<TestType> ()); 
        REQUIRE(dataBack.size() == data.size());
        for (int i = 0; i < nSamples; ++i)
        {
            REQUIRE(dataBack.at(i) == data.at(i));
        }
    }
    else if (std::is_same<TestType, int64_t>::value)
    {
        auto nBackSamples = result.number_of_samples();
        auto dataBack = ::unpack<TestType>(result.data(), nBackSamples);
        REQUIRE(result.data_type() == ::toOutputDataType<TestType> ());
        REQUIRE(dataBack.size() == data.size());
        for (int i = 0; i < nSamples; ++i)
        {
            REQUIRE(dataBack.at(i) == data.at(i));
        }
    }
    else if (std::is_same<TestType, float>::value)
    {   
        auto nBackSamples = result.number_of_samples();
        auto dataBack = ::unpack<TestType>(result.data(), nBackSamples);
        REQUIRE(result.data_type() == ::toOutputDataType<TestType> ());
        REQUIRE(dataBack.size() == data.size());
        constexpr float tolerance{std::numeric_limits<TestType>::epsilon()*10};
        for (int i = 0; i < nSamples; ++i)
        {
            REQUIRE_THAT(static_cast<float> (dataBack.at(i)),
                Catch::Matchers::WithinRel(
                   static_cast<float> (data.at(i)), tolerance));
        }
    }   
    else if (std::is_same<TestType, double>::value)
    {   
        auto nBackSamples = result.number_of_samples();
        auto dataBack = ::unpack<TestType>(result.data(), nBackSamples);
        REQUIRE(result.data_type() == ::toOutputDataType<TestType> ());
        REQUIRE(dataBack.size() == data.size());
        constexpr double tolerance{std::numeric_limits<TestType>::epsilon()*10};
        for (int i = 0; i < nSamples; ++i)
        {
            REQUIRE_THAT(static_cast<double> (dataBack.at(i)),
                Catch::Matchers::WithinRel(
                   static_cast<double> (data.at(i)), tolerance));
        }   
    }
}
