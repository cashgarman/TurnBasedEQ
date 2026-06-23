#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
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
    CastSpell = 4,
    UseAbility = 5,
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
    Heal = 11,
    SpellCast = 12,
    SpellFizzle = 13,
    SpellResist = 14,
    AbilityUsed = 15,
    StatusApplied = 16,
    StatusTick = 17,
    ManaSpend = 18,
};

enum class StatusEffectType : uint8_t
{
    Stun = 0,
    Snare = 1,
    Dot = 2,
};

struct StatusEffect
{
    StatusEffectType type = StatusEffectType::Stun;
    uint16_t remainingTurns = 0;
    int32_t tickValue = 0;
    std::string sourceName;
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
    std::string classId;
    std::string raceId;
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
    uint16_t channelingSkillLevel = 1;
    std::unordered_map<std::string, uint16_t> skillLevels;
    std::vector<std::string> unlockedSpells;
    std::vector<std::string> unlockedAbilities;
    std::vector<StatusEffect> statusEffects;
    bool isAlive = true;
    bool isDefending = false;
    bool isPlayerControlled = false;
    bool isAiCompanion = false;
    bool godMode = false;
    bool isStunned() const;
    bool isSnared() const;
    uint16_t skillLevel(const std::string& skillId) const;
};

struct CombatActionIntent
{
    CombatActionType action = CombatActionType::None;
    uint32_t targetSlot = 0;
    std::string spellId;
    std::string abilityId;
};

} // namespace tbeq::combat
