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
    std::string equippedWeaponItemId;
    std::string equippedHeadItemId;
    std::string equippedChestItemId;
    std::string equippedHandsItemId;
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
    bool isAiCompanion = false;
    std::string classId;
    std::string raceId;
    std::string mobId;
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
    std::string spellId;
    std::string abilityId;
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

struct MeditateRequestPayload
{
};

struct MeditateResultPayload
{
    bool ok = false;
    std::string message;
    uint16_t manaGained = 0;
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

struct LevelUpPayload
{
    uint16_t oldLevel = 1;
    uint16_t newLevel = 1;
    uint32_t experienceGranted = 0;
    std::string message;
};

struct SkillEntryPayload
{
    std::string skillId;
    uint16_t level = 1;
    uint32_t experience = 0;
    uint16_t cap = 0;
};

struct SkillsSnapshotPayload
{
    uint16_t characterLevel = 1;
    uint32_t characterExperience = 0;
    uint32_t experienceToNextLevel = 0;
    std::vector<SkillEntryPayload> skills;
};

struct InventoryEntryPayload
{
    std::string itemId;
    uint16_t quantity = 0;
};

struct EquipmentEntryPayload
{
    std::string slot;
    std::string itemId;
};

struct InventorySnapshotPayload
{
    uint32_t gold = 0;
    std::vector<InventoryEntryPayload> items;
    std::vector<EquipmentEntryPayload> equipment;
};

struct EquipItemRequestPayload
{
    std::string itemId;
};

struct EquipItemResultPayload
{
    bool ok = false;
    std::string message;
};

struct UnequipItemRequestPayload
{
    std::string slot;
};

struct UnequipItemResultPayload
{
    bool ok = false;
    std::string message;
};

struct NpcInteractRequestPayload
{
    uint32_t npcEntityId = 0;
};

struct MerchantStockEntryPayload
{
    std::string itemId;
    uint16_t quantity = 0;
    uint32_t buyPrice = 0;
    uint32_t sellPrice = 0;
};

struct MerchantOpenPayload
{
    uint32_t npcEntityId = 0;
    std::string npcName;
    std::vector<MerchantStockEntryPayload> stock;
    std::vector<std::string> sellItemIds;
};

struct MerchantBuyRequestPayload
{
    uint32_t npcEntityId = 0;
    std::string itemId;
    uint16_t quantity = 1;
};

struct MerchantBuyResultPayload
{
    bool ok = false;
    std::string message;
    bool stockUpdated = false;
    std::string stockItemId;
    uint16_t stockQuantity = 0;
};

struct MerchantSellRequestPayload
{
    uint32_t npcEntityId = 0;
    std::string itemId;
    uint16_t quantity = 1;
};

struct MerchantSellResultPayload
{
    bool ok = false;
    std::string message;
};

struct NpcDialogOpenPayload
{
    uint32_t npcEntityId = 0;
    std::string npcName;
    std::vector<std::string> lines;
};

struct SessionEndPayload
{
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
