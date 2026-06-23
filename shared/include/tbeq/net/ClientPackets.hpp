#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tbeq/net/DebugCommands.hpp"
#include "tbeq/net/PacketTypes.hpp"

namespace tbeq::net
{

struct ClientPingPayload
{
    uint64_t timestampMs = 0;
};

struct ClientPongPayload
{
    uint64_t timestampMs = 0;
};

struct LoginRequestPayload
{
    std::string username;
    std::string password;
};

struct LoginResponsePayload
{
    bool ok = false;
    std::string message;
    uint64_t sessionToken = 0;
    uint64_t sessionTokenHash = 0;
};

struct CreateAccountRequestPayload
{
    std::string username;
    std::string password;
};

struct CreateAccountResponsePayload
{
    bool ok = false;
    std::string message;
};

struct CharacterSummaryPayload
{
    std::string characterId;
    std::string name;
    std::string raceId;
    std::string classId;
    uint16_t level = 1;
    std::string zoneId;
};

struct CharacterListRequestPayload
{
};

struct CharacterListResponsePayload
{
    bool ok = false;
    std::string message;
    std::vector<CharacterSummaryPayload> characters;
};

struct CreateCharacterRequestPayload
{
    std::string name;
    std::string raceId;
    std::string classId;
};

struct CreateCharacterResponsePayload
{
    bool ok = false;
    std::string message;
    std::string characterId;
};

struct SelectCharacterRequestPayload
{
    std::string characterId;
};

struct ZoneConnectInfoPayload
{
    bool ok = false;
    std::string message;
    std::string zoneId;
    std::string zoneHost;
    uint16_t zonePort = 0;
    uint64_t sessionResumeToken = 0;
    std::string characterId;
};

struct SessionResumePayload
{
    std::string characterId;
    uint64_t sessionResumeToken = 0;
};

struct SessionResumeResponsePayload
{
    bool ok = false;
    std::string message;
    uint32_t entityId = 0;
    int32_t tileX = 0;
    int32_t tileY = 0;
};

struct ZoneSnapshotPayload
{
    std::string zoneId;
    std::string zoneName;
    std::string tileStyle;
    int32_t width = 0;
    int32_t height = 0;
    std::vector<std::string> tiles;
};

struct EntityStatePayload
{
    uint32_t entityId = 0;
    std::string name;
    uint8_t entityType = 0;
    int32_t tileX = 0;
    int32_t tileY = 0;
};

struct EntitySnapshotPayload
{
    std::vector<EntityStatePayload> entities;
};

struct MoveIntentPayload
{
    int32_t targetTileX = 0;
    int32_t targetTileY = 0;
};

struct MoveResultPayload
{
    bool ok = false;
    std::string message;
    uint32_t entityId = 0;
    int32_t tileX = 0;
    int32_t tileY = 0;
};

struct ChatMessagePayload
{
    uint8_t channel = 0;
    std::string text;
};

struct ChatDeliverPayload
{
    uint8_t channel = 0;
    std::string senderName;
    std::string text;
};

struct UsePortalPayload
{
};

struct UsePortalResultPayload
{
    bool ok = false;
    std::string message;
};

struct RequestZoneTilesPayload
{
};

struct ZoneTileGridPayload
{
    std::string zoneId;
    std::vector<std::string> tiles;
};

struct ZoneRegisterPayload
{
    std::string zoneId;
    std::string listenHost;
    uint16_t listenPort = 0;
    uint32_t capacity = 100;
    std::string version;
};

struct ZoneRegisterAckPayload
{
    bool accepted = false;
    std::string message;
};

using ClientDebugCommandRequestPayload = DebugCommandRequestPayload;
using ClientDebugCommandResponsePayload = DebugCommandResponsePayload;

} // namespace tbeq::net
