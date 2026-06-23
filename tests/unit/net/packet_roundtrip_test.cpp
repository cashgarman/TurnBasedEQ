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

TEST_CASE("EntityStatePayload round-trip with appearance fields", "[net]")
{
    tbeq::net::EntityStatePayload original;
    original.entityId = 42;
    original.name = "DemoHero";
    original.entityType = 0;
    original.tileX = 12;
    original.tileY = 34;
    original.raceId = "wood_elf";
    original.classId = "wizard";
    original.appearanceId = "";

    const auto writer = tbeq::net::serialize(original);
    const auto packet = tbeq::net::serializeClientPacket(
        tbeq::net::ClientPacketType::EntitySnapshot,
        11,
        tbeq::net::serialize(tbeq::net::EntitySnapshotPayload{{original}}));
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::EntitySnapshotPayload parsedSnapshot;
    REQUIRE(tbeq::net::deserializeClientPacket(decoded, parsedSnapshot));
    REQUIRE(parsedSnapshot.entities.size() == 1);

    const auto& parsed = parsedSnapshot.entities.front();
    REQUIRE(parsed.entityId == original.entityId);
    REQUIRE(parsed.name == original.name);
    REQUIRE(parsed.entityType == original.entityType);
    REQUIRE(parsed.tileX == original.tileX);
    REQUIRE(parsed.tileY == original.tileY);
    REQUIRE(parsed.raceId == original.raceId);
    REQUIRE(parsed.classId == original.classId);
    REQUIRE(parsed.appearanceId == original.appearanceId);
}

TEST_CASE("EntityStatePayload legacy payload deserializes with defaults", "[net]")
{
    tbeq::net::ByteWriter writer;
    writer.writeU32(7);
    writer.writeString("Merchant");
    writer.writeU8(1);
    writer.writeU32(20);
    writer.writeU32(28);

    tbeq::net::ByteReader reader(writer.data());
    tbeq::net::EntityStatePayload parsed;
    REQUIRE(tbeq::net::deserializeEntityState(reader, parsed));
    REQUIRE(parsed.entityId == 7);
    REQUIRE(parsed.name == "Merchant");
    REQUIRE(parsed.entityType == 1);
    REQUIRE(parsed.tileX == 20);
    REQUIRE(parsed.tileY == 28);
    REQUIRE(parsed.raceId.empty());
    REQUIRE(parsed.classId.empty());
    REQUIRE(parsed.appearanceId.empty());
}

TEST_CASE("PlayerDisconnect server packet round-trip", "[net]")
{
    tbeq::net::PlayerDisconnectPayload original;
    original.characterId = "char_123";

    const auto writer = tbeq::net::serialize(original);
    const auto packet = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::PlayerDisconnect,
        11,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::PlayerDisconnectPayload parsed;
    REQUIRE(tbeq::net::deserializeServerPacket(decoded, parsed));
    REQUIRE(parsed.characterId == original.characterId);
}

TEST_CASE("SessionEnd client packet round-trip", "[net]")
{
    const auto writer = tbeq::net::serialize(tbeq::net::SessionEndPayload{});
    const auto packet = tbeq::net::serializeClientPacket(
        tbeq::net::ClientPacketType::SessionEnd,
        12,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::SessionEndPayload parsed;
    REQUIRE(tbeq::net::deserializeClientPacket(decoded, parsed));
}

TEST_CASE("PlayerCommand client packet round-trip", "[net]")
{
    tbeq::net::PlayerCommandRequestPayload request;
    request.command = "stuck";
    request.args = {"extra"};

    const auto requestWriter = tbeq::net::serialize(request);
    const auto requestPacket = tbeq::net::serializeClientPacket(
        tbeq::net::ClientPacketType::PlayerCommandRequest,
        13,
        requestWriter);
    const auto requestEncoded = tbeq::net::encodePacket(requestPacket);

    tbeq::net::SerializedPacket requestDecoded;
    REQUIRE(tbeq::net::decodePacket(requestEncoded, requestDecoded));

    tbeq::net::PlayerCommandRequestPayload parsedRequest;
    REQUIRE(tbeq::net::deserializeClientPacket(requestDecoded, parsedRequest));
    REQUIRE(parsedRequest.command == request.command);
    REQUIRE(parsedRequest.args == request.args);

    tbeq::net::PlayerCommandResultPayload result;
    result.ok = true;
    result.message = "You have been relocated to zone center.";
    result.tileX = 32;
    result.tileY = 32;

    const auto resultWriter = tbeq::net::serialize(result);
    const auto resultPacket = tbeq::net::serializeClientPacket(
        tbeq::net::ClientPacketType::PlayerCommandResult,
        14,
        resultWriter);
    const auto resultEncoded = tbeq::net::encodePacket(resultPacket);

    tbeq::net::SerializedPacket resultDecoded;
    REQUIRE(tbeq::net::decodePacket(resultEncoded, resultDecoded));

    tbeq::net::PlayerCommandResultPayload parsedResult;
    REQUIRE(tbeq::net::deserializeClientPacket(resultDecoded, parsedResult));
    REQUIRE(parsedResult.ok);
    REQUIRE(parsedResult.message == result.message);
    REQUIRE(parsedResult.tileX == result.tileX);
    REQUIRE(parsedResult.tileY == result.tileY);
}

TEST_CASE("ZoneTransferComplete server packet round-trip", "[net]")
{
    tbeq::net::ZoneTransferCompletePayload original;
    original.characterId = "char_456";
    original.zoneId = "starter_hunting";
    original.posX = 64.0f;
    original.posY = 8.0f;

    const auto writer = tbeq::net::serialize(original);
    const auto packet = tbeq::net::serializeServerPacket(
        tbeq::net::ServerPacketType::ZoneTransferComplete,
        12,
        writer);
    const auto encoded = tbeq::net::encodePacket(packet);

    tbeq::net::SerializedPacket decoded;
    REQUIRE(tbeq::net::decodePacket(encoded, decoded));

    tbeq::net::ZoneTransferCompletePayload parsed;
    REQUIRE(tbeq::net::deserializeServerPacket(decoded, parsed));
    REQUIRE(parsed.characterId == original.characterId);
    REQUIRE(parsed.zoneId == original.zoneId);
    REQUIRE(parsed.posX == original.posX);
    REQUIRE(parsed.posY == original.posY);
}
