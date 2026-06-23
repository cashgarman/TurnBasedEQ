#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq::combat
{

enum class CombatSide : uint8_t
{
    Player = 0,
    Enemy = 1,
};

enum class CombatPhase : uint8_t
{
    SelectAction = 0,
    Resolving = 1,
    Ended = 2,
};

enum class CombatActionType : uint8_t
{
    None = 0,
    MeleeAttack = 1,
    Defend = 2,
    Flee = 3,
};

enum class CombatOutcome : uint8_t
{
    InProgress = 0,
    Victory = 1,
    Defeat = 2,
    Fled = 3,
};

enum class CombatEventType : uint8_t
{
    Damage = 0,
    Miss = 1,
    Defend = 2,
    FleeAttempt = 3,
    FleeSuccess = 4,
    FleeFailed = 5,
    Death = 6,
    SkillGain = 7,
    Loot = 8,
    CombatEnd = 9,
    TurnStart = 10,
};

struct CombatEvent
{
    CombatEventType type = CombatEventType::Damage;
    uint32_t actorSlot = 0;
    uint32_t targetSlot = 0;
    int32_t value = 0;
    std::string message;
    std::string detail;
};

struct CombatParticipant
{
    uint32_t slot = 0;
    std::string characterId;
    std::string mobId;
    std::string name;
    CombatSide side = CombatSide::Player;
    uint16_t level = 1;
    uint16_t hp = 0;
    uint16_t maxHp = 0;
    uint16_t mana = 0;
    uint16_t maxMana = 0;
    uint16_t agi = 75;
    uint16_t offense = 10;
    uint16_t defense = 10;
    std::string weaponSkillId = "1h_slash";
    uint16_t weaponSkillLevel = 1;
    uint16_t offenseSkillLevel = 1;
    uint16_t defenseSkillLevel = 1;
    bool isAlive = true;
    bool isDefending = false;
    bool isPlayerControlled = false;
    bool godMode = false;
};

} // namespace tbeq::combat
