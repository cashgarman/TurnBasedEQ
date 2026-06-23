#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"

TEST_CASE("Server packet round-trip ZoneRegister", "[net]")
{
    tbeq::net::ZoneRegisterPayload original;
    original.zoneId = "starter_city";
    original.listenHost = "127.0.0.1";
    original.listenPort = 9100;
    original.capacity = 64;
    original.version = "0.1.0";

    const auto writer = tbeq::net::serialize(original);
    const auto packet = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::ZoneRegister,
        42,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::ZoneRegisterPayload parsed;
    REQUIRE(tbeq::net::deserializeServerPacket(decoded, parsed));
    REQUIRE(parsed.zoneId == original.zoneId);
    REQUIRE(parsed.listenHost == original.listenHost);
    REQUIRE(parsed.listenPort == original.listenPort);
    REQUIRE(parsed.capacity == original.capacity);
    REQUIRE(parsed.version == original.version);
}

TEST_CASE("Server Ping/Pong round-trip", "[net]")
{
    tbeq::net::ServerPingPayload ping{123456789ULL};
    const auto writer = tbeq::net::serialize(ping);
    const auto packet = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::Ping,
        7,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::ServerPingPayload parsedPing;
    REQUIRE(tbeq::net::deserializeServerPacket(decoded, parsedPing));
    REQUIRE(parsedPing.timestampMs == ping.timestampMs);

    tbeq::net::ServerPongPayload pong{parsedPing.timestampMs};
    const auto pongWriter = tbeq::net::serialize(pong);
    const auto pongPacket = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::Pong,
        8,
        pongWriter);
    const auto pongEncoded = tbeq::net::encodePacket(pongPacket);

    tbeq::net::SerializedPacket decodedPong;
    REQUIRE(tbeq::net::decodePacket(pongEncoded, decodedPong));
    tbeq::net::ServerPongPayload parsedPong;
    REQUIRE(tbeq::net::deserializeServerPacket(decodedPong, parsedPong));
    REQUIRE(parsedPong.timestampMs == ping.timestampMs);
}

TEST_CASE("Debug command request round-trip", "[net]")
{
    tbeq::net::ServerDebugCommandRequestPayload request;
    request.command = tbeq::net::DebugCommand::Ping;
    request.args = {"arg1", "arg2"};

    const auto writer = tbeq::net::serialize(request);
    const auto packet = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::DebugCommandRequest,
        99,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::ServerDebugCommandRequestPayload parsed;
    REQUIRE(tbeq::net::deserializeServerPacket(decoded, parsed));
    REQUIRE(parsed.command == request.command);
    REQUIRE(parsed.args == request.args);
}

TEST_CASE("Invalid packet header rejected", "[net]")
{
    tbeq::net::PacketHeader header{};
    header.magic = 0xDEADBEEF;
    std::vector<uint8_t> bytes(sizeof(header));
    std::memcpy(bytes.data(), &header, sizeof(header));

    tbeq::net::PacketHeader parsed;
    REQUIRE_FALSE(tbeq::net::readPacketHeader(bytes, parsed));
}
