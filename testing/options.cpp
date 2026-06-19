#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <catch2/catch_test_macros.hpp>
//#include <catch2/matchers/catch_matchers.hpp>
//#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "uDataPacketCacheService/circularBufferOptions.hpp"
#include "uDataPacketCacheService/grpcClientOptions.hpp"
#include "uDataPacketCacheService/grpcServerOptions.hpp"

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

TEST_CASE("UDataPacketCacheService", "[grpcClientOptions]")
{
    SECTION("Defaults")
    {   
        const GRPCClientOptions options;
        REQUIRE(options.getHost() == "localhost");
        REQUIRE(options.getPort() == 50000);
        REQUIRE(options.getAccessToken() == std::nullopt);
        REQUIRE(options.getServerCertificate() == std::nullopt);
        REQUIRE(options.getClientCertificate() == std::nullopt);
        REQUIRE(options.getClientKey() == std::nullopt);
        const std::vector<std::chrono::milliseconds> schedule
        {
            std::chrono::seconds {0},
            std::chrono::seconds {5},
            std::chrono::seconds {15},
            std::chrono::seconds {30}
        };
        auto reconnectSchedule = options.getReconnectSchedule();
        REQUIRE(schedule.size() == reconnectSchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == reconnectSchedule.at(i));
        }
    }   

    SECTION("Options")
    {   
        const std::string host{"some.host.org"};
        const std::string token{"super-secret-token"};
        const std::string serverCertificate{"some-wonky-hash"};
        const std::string clientCertificate{"some-other-hash"};
        const std::string clientKey{"some-private-hash"};
        const uint16_t port{12345};
        const std::vector<std::chrono::milliseconds> schedule
        {
            std::chrono::seconds {2},
            std::chrono::seconds {3},
            std::chrono::seconds {4}
        };

        GRPCClientOptions options;
        options.setHost(host);
        options.setPort(port);
        options.setServerCertificate(serverCertificate);
        options.setAccessToken(token);
        options.setClientCertificate(clientCertificate);
        options.setClientKey(clientKey);
        REQUIRE_NOTHROW(options.setReconnectSchedule(schedule));

        REQUIRE(options.getHost() == host);
        REQUIRE(options.getPort() == port);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*options.getServerCertificate() == serverCertificate);
        REQUIRE(*options.getAccessToken() == token);
        REQUIRE(*options.getClientCertificate() == clientCertificate);
        REQUIRE(*options.getClientKey() == clientKey);
        //NOLINTEND(bugprone-unchecked-optional-access)
        auto reconnectSchedule = options.getReconnectSchedule();
        REQUIRE(schedule.size() == reconnectSchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == reconnectSchedule.at(i));
        }
    }
}

TEST_CASE("UDataPacketCacheService", "[grpcServerOptions]")
{
    SECTION("Defaults")
    {   
        const GRPCServerOptions options;
        REQUIRE(options.getHost() == "localhost");
        REQUIRE(options.getPort() == 50000);
        REQUIRE(options.getAccessToken() == std::nullopt);
        REQUIRE(options.getServerCertificate() == std::nullopt);
        REQUIRE(options.getServerKey() == std::nullopt);
        REQUIRE(options.getClientCertificate() == std::nullopt);
        REQUIRE(options.isReflectionEnabled() == false);
    }   

    SECTION("Options")
    {
        const std::string host{"some.host.org"};
        const std::string token{"super-secret-token"};
        const std::string serverCertificate{"some-wonky-hash"};
        const std::string serverKey{"some-private-wonky-hash"};
        const std::string clientCertificate{"some-rad-hash"};
        constexpr uint16_t port{12345};
        GRPCServerOptions options;

        options.setHost(host);
        options.setPort(port);
        options.setServerCertificate(serverCertificate);
        options.setServerKey(serverKey);
        options.setClientCertificate(clientCertificate);
        options.setAccessToken(token);
        options.enableReflection();

        const GRPCServerOptions copy{options};
        REQUIRE(copy.getHost() == host);
        REQUIRE(copy.getPort() == port);
        REQUIRE(copy.getServerCertificate() != std::nullopt);
        REQUIRE(copy.getServerKey() != std::nullopt);
        REQUIRE(copy.getClientCertificate() != std::nullopt);
        REQUIRE(copy.getAccessToken() != std::nullopt);
        REQUIRE(*copy.getServerCertificate() == serverCertificate); //NOLINT
        REQUIRE(*copy.getServerKey() == serverKey); //NOLINT
        REQUIRE(*copy.getClientCertificate() == clientCertificate); //NOLINT
        REQUIRE(*copy.getAccessToken() == token); //NOLINT
        REQUIRE(copy.isReflectionEnabled() == true);
    }
}
