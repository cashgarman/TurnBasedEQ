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
    std::string raceId;
    std::string classId;
    std::string appearanceId;
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

struct CombatParticipantPayload
{
    uint32_t combatSlot = 0;
    std::string name;
    uint8_t side = 0;
    uint16_t hp = 0;
    uint16_t maxHp = 0;
    uint16_t mana = 0;
    uint16_t maxMana = 0;
    bool isAlive = true;
    bool isPlayerControlled = false;
};

struct CombatStartPayload
{
    uint32_t combatId = 0;
    std::vector<CombatParticipantPayload> participants;
    std::vector<uint32_t> turnOrder;
    uint32_t currentTurnIndex = 0;
    uint32_t currentActorSlot = 0;
    uint32_t turnDurationMs = 0;
};

struct CombatUpdatePayload
{
    uint32_t combatId = 0;
    uint32_t currentTurnIndex = 0;
    uint32_t currentActorSlot = 0;
    uint32_t turnDurationMs = 0;
    std::vector<CombatParticipantPayload> participants;
};

struct CombatEventPayload
{
    uint32_t combatId = 0;
    uint8_t eventType = 0;
    uint32_t actorSlot = 0;
    uint32_t targetSlot = 0;
    int32_t value = 0;
    std::string message;
    std::string detail;
};

struct CombatEndPayload
{
    uint32_t combatId = 0;
    uint8_t result = 0;
    std::string message;
};

struct SubmitActionPayload
{
    uint32_t combatId = 0;
    uint8_t actionType = 0;
    uint32_t targetCombatSlot = 0;
};

struct SubmitActionResultPayload
{
    bool ok = false;
    std::string message;
};

struct CharacterVitalsPayload
{
    uint16_t hp = 0;
    uint16_t maxHp = 0;
    uint16_t mana = 0;
    uint16_t maxMana = 0;
};

struct SkillGainPayload
{
    std::string skillId;
    uint16_t oldLevel = 0;
    uint16_t newLevel = 0;
    std::string message;
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
