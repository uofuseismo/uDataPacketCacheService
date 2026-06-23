#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <catch2/catch_test_macros.hpp>
//#include <catch2/matchers/catch_matchers.hpp>
//#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include "uDataPacketCacheService/circularBufferOptions.hpp"
#include "uDataPacketCacheService/grpcClientOptions.hpp"
#include "uDataPacketCacheService/grpcServerOptions.hpp"
#include "uDataPacketCacheService/serviceOptions.hpp"
#include "uDataPacketCacheService/subscriberOptions.hpp"

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

TEST_CASE("UDataPacketCacheService", "[SubscriberOptions]")
{
    const std::string identifier{"grpc-client-12"};
    const std::string host{"some.host.org"};
    const uint16_t port{12345};

    UDataPacketServiceAPI::V1::StreamIdentifier id1;
    id1.set_network("UU");
    id1.set_station("CTU");
    id1.set_channel("HHZ");
    id1.set_location_code("01");

    UDataPacketServiceAPI::V1::StreamIdentifier id2;
    id2.set_network("UU");
    id2.set_station("ELU");
    id2.set_channel("EHZ");
    id2.set_location_code("01");

    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers
    {   
        id1, id2 
    };  

    GRPCClientOptions grpcOptions;
    grpcOptions.setHost(host);
    grpcOptions.setPort(port);

    SubscriberOptions options;
    //NOLINTBEGIN(bugprone-unchecked-optional-access)
    REQUIRE(*options.getIdentifier() == "uDataPacketCacheService");
    //NOLINTEND(bugprone-unchecked-optional-access)

    REQUIRE_NOTHROW(options.setGRPCOptions(grpcOptions));
    REQUIRE_NOTHROW(options.setStreamIdentifiers(streamIdentifiers));
    options.setIdentifier(identifier);

    REQUIRE(options.getGRPCOptions().getHost() == host);
    REQUIRE(options.getGRPCOptions().getPort() == port);
    //NOLINTBEGIN(bugprone-unchecked-optional-access)
    REQUIRE(*options.getIdentifier() == identifier);
    //NOLINTEND(bugprone-unchecked-optional-access)
    REQUIRE(options.getStreamIdentifiers().size() == streamIdentifiers.size());
    REQUIRE(options.getStreamIdentifiers().size() == streamIdentifiers.size());
    for (const auto &id : options.getStreamIdentifiers())
    {
        bool matched{false};
        for (const auto &jd : streamIdentifiers)
        {
            if (id.network() == jd.network() &&
                id.station() == jd.station() &&
                id.channel() == jd.channel() &&
                id.location_code() == jd.location_code())
            {
                matched = true;
            }
       }
       REQUIRE(matched);
    }

    streamIdentifiers.push_back(id1);
    REQUIRE_THROWS(options.setStreamIdentifiers(streamIdentifiers));
}

TEST_CASE("UDataPacketCacheService", "[ServiceOptions]")
{
    SECTION("Defaults")
    {
        const ServiceOptions options;
        REQUIRE_FALSE(options.hasGRPCOptions());
        REQUIRE(options.getMaximumQueueSize() == 32);
        REQUIRE(options.getMaximumRequestMessageSizeInBytes() == 1024);
    }

    SECTION("Options")
    {
        const std::string host{"localhost"};
        constexpr uint16_t port{12343};
        constexpr int maxQueueSize{19};
        constexpr int maxMessageSize{301};
        GRPCServerOptions grpcOptions;
        grpcOptions.setHost(host);
        grpcOptions.setPort(port);
      
        ServiceOptions options;
        REQUIRE_THROWS(options.setMaximumQueueSize(0));
        REQUIRE_THROWS(options.setMaximumRequestMessageSizeInBytes(0));
        options.setGRPCOptions(grpcOptions);
        options.setMaximumQueueSize(maxQueueSize);
        options.setMaximumRequestMessageSizeInBytes(maxMessageSize);

        const ServiceOptions copy{options};
        REQUIRE(copy.getGRPCOptions().getHost() == host);
        REQUIRE(copy.getGRPCOptions().getPort() == port);
        REQUIRE(copy.getMaximumQueueSize() == maxQueueSize);
        REQUIRE(copy.getMaximumRequestMessageSizeInBytes() == maxMessageSize);
    }

}


