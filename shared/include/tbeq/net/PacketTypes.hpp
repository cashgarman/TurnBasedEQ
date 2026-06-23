#pragma once

#include <cstdint>

namespace tbeq::net
{

enum class ClientPacketType : uint16_t
{
    Ping = 1,
    Pong = 2,
    LoginRequest = 10,
    LoginResponse = 11,
    CreateAccountRequest = 12,
    CreateAccountResponse = 13,
    CharacterListRequest = 20,
    CharacterListResponse = 21,
    CreateCharacterRequest = 22,
    CreateCharacterResponse = 23,
    SelectCharacterRequest = 24,
    ZoneConnectInfo = 25,
    SessionResume = 30,
    SessionResumeResponse = 31,
    ZoneSnapshot = 32,
    EntitySnapshot = 33,
    MoveIntent = 34,
    MoveResult = 35,
    ChatMessage = 36,
    ChatDeliver = 37,
    UsePortal = 38,
    UsePortalResult = 39,
    RequestZoneTiles = 40,
    ZoneTileGrid = 41,
    CombatStart = 50,
    CombatUpdate = 51,
    CombatEvent = 52,
    CombatEnd = 53,
    SubmitAction = 54,
    SubmitActionResult = 55,
    CharacterVitals = 56,
    SkillGain = 57,
    MeditateRequest = 58,
    MeditateResult = 59,
    InventorySnapshot = 60,
    EquipItemRequest = 61,
    EquipItemResult = 62,
    UnequipItemRequest = 63,
    UnequipItemResult = 64,
    NpcInteractRequest = 65,
    MerchantOpen = 66,
    MerchantBuyRequest = 67,
    MerchantBuyResult = 68,
    MerchantSellRequest = 69,
    MerchantSellResult = 70,
    NpcDialogOpen = 71,
    SessionEnd = 72,
    DebugCommandRequest = 100,
    DebugCommandResponse = 101,
};

enum class ServerPacketType : uint16_t
{
    Ping = 1,
    Pong = 2,
    ZoneRegister = 10,
    ZoneRegisterAck = 11,
    PlayerEnterPrepare = 20,
    PlayerEnterReady = 21,
    LoadCharacterRequest = 30,
    LoadCharacterResponse = 31,
    ZoneTransferRequest = 40,
    ZoneTransferAuthorize = 41,
    ZoneTransferComplete = 42,
    PlayerDisconnect = 43,
    ZoneConnectForward = 44,
    DebugCommandRequest = 100,
    DebugCommandResponse = 101,
};

} // namespace tbeq::net
