#pragma once

#include <cstdint>
#include <string>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/DebugCommands.hpp"
#include "tbeq/net/PacketTypes.hpp"

namespace tbeq::net
{

struct ServerPingPayload
{
    uint64_t timestampMs = 0;
};

struct ServerPongPayload
{
    uint64_t timestampMs = 0;
};

struct PlayerEnterPreparePayload
{
    std::string characterId;
    int64_t accountId = 0;
    std::string zoneId;
    uint64_t sessionResumeToken = 0;
};

struct PlayerEnterReadyPayload
{
    std::string characterId;
    bool ok = false;
    std::string message;
};

struct LoadCharacterRequestPayload
{
    std::string characterId;
};

struct LoadCharacterResponsePayload
{
    bool ok = false;
    std::string message;
    std::string characterId;
    std::string name;
    std::string raceId;
    std::string classId;
    uint16_t level = 1;
    std::string zoneId;
    float posX = 0.0f;
    float posY = 0.0f;
    std::string stateJson;
};

struct ZoneTransferRequestPayload
{
    std::string characterId;
    std::string sourceZoneId;
    std::string destZoneId;
    int32_t destTileX = 0;
    int32_t destTileY = 0;
    uint64_t sessionResumeToken = 0;
};

struct ZoneTransferAuthorizePayload
{
    std::string characterId;
};

struct ZoneTransferCompletePayload
{
    std::string characterId;
    std::string zoneId;
    float posX = 0.0f;
    float posY = 0.0f;
};

struct PlayerDisconnectPayload
{
    std::string characterId;
};

struct ZoneConnectForwardPayload
{
    bool ok = false;
    std::string message;
    std::string zoneId;
    std::string zoneHost;
    uint16_t zonePort = 0;
    uint64_t sessionResumeToken = 0;
    std::string characterId;
};

using ServerDebugCommandRequestPayload = DebugCommandRequestPayload;
using ServerDebugCommandResponsePayload = DebugCommandResponsePayload;

} // namespace tbeq::net
