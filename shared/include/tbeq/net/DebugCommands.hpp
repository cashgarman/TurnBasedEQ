#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq::net
{

enum class DebugCommand : uint16_t
{
    Ping = 1,
    SubscribeLogs = 2,
    ResetUILayout = 3,
    SpawnMob = 10,
    KillTarget = 11,
    GodMode = 12,
    ForceCombatEnd = 13,
    UnlockAllSpells = 14,
    FillMana = 15,
    SpawnAi = 16,
    GrantItem = 17,
    EquipItem = 18,
    SetSkillLevel = 19,
    MaxSkills = 20,
    GrantExperience = 21,
    PracticeSkill = 22,
    ListZones = 23,
    TeleportToZone = 24,
    Unknown = 9999,
};

struct DebugCommandRequestPayload
{
    DebugCommand command = DebugCommand::Unknown;
    std::vector<std::string> args;
};

struct DebugCommandResponsePayload
{
    bool ok = false;
    std::string message;
};

} // namespace tbeq::net
