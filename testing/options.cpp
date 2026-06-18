//#include <chrono>
#include <optional>
#include <catch2/catch_test_macros.hpp>
//#include <catch2/matchers/catch_matchers.hpp>
//#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketCacheService/circularBufferOptions.hpp"

using namespace UDataPacketCacheService;

TEST_CASE("UDataPacketCacheService::Options", "[circularBufferOptions]")
{
    SECTION("Default")
    {
        constexpr int maxPackets{150};
        const CircularBufferOptions options;
        REQUIRE(options.getMaximumNumberOfPackets() == maxPackets);
        //REQUIRE(options.getMaximumDuration() == duration);
    }

    SECTION("Options")
    {
        constexpr int maxPackets{10};
        //constexpr std::chrono::nanoseconds duration{std::chrono::seconds{44}};
        CircularBufferOptions options;
        REQUIRE_THROWS(options.setMaximumNumberOfPackets(-1));
        REQUIRE_THROWS(options.setMaximumNumberOfPackets(0));
        REQUIRE_NOTHROW(options.setMaximumNumberOfPackets(maxPackets));
        //REQUIRE_NOTHROW(options.setMaximumDuration(duration));
 
        const CircularBufferOptions copy{options};
        REQUIRE(copy.getMaximumNumberOfPackets() == maxPackets);
        //REQUIRE(copy.getMaximumDuration() == duration);
    }
}

