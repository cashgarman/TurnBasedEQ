#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

#include "tbeq/content/SkillCapCatalog.hpp"
#include "tbeq/skills/SkillDef.hpp"

namespace tbeq
{

struct SkillProgress
{
    uint16_t level = 1;
    uint32_t experience = 0;
};

class SkillResolver
{
public:
    void setSkillCapCatalog(const content::SkillCapCatalog* catalog);

    uint16_t getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const;
    bool canUseSkill(const SkillProgress& progress, uint16_t cap) const;
    bool applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap);

    bool rollMeleeHit(
        uint16_t offenseSkill,
        uint16_t weaponSkill,
        uint16_t targetDefense,
        std::mt19937& rng) const;

    int32_t calculateMeleeDamage(
        uint16_t attackerLevel,
        uint16_t weaponSkill,
        uint16_t offenseSkill,
        uint16_t targetDefense,
        std::mt19937& rng,
        float damageMultiplier = 1.0f) const;

    bool rollFlee(
        uint16_t defenseSkill,
        uint16_t agility,
        int32_t livingEnemyCount,
        std::mt19937& rng) const;

    bool rollChanneling(uint16_t channelingSkill, std::mt19937& rng) const;

    bool rollSpellResist(
        uint16_t casterSchoolSkill,
        uint16_t targetLevel,
        std::mt19937& rng) const;

    bool rollManeuver(
        uint16_t maneuverSkill,
        uint16_t targetLevel,
        std::mt19937& rng) const;

    int32_t calculateSpellDamage(
        int32_t baseValue,
        uint16_t schoolSkill,
        uint16_t casterLevel) const;

    int32_t calculateHealAmount(
        int32_t baseValue,
        uint16_t schoolSkill,
        uint16_t casterLevel) const;

    int32_t calculateAbilityDamage(
        int32_t baseValue,
        uint16_t linkedSkillLevel,
        uint16_t offenseSkill,
        uint16_t attackerLevel) const;

    uint16_t meditateManaGain(uint16_t meditateSkill, uint16_t maxMana) const;

    uint32_t combatSkillXpGain(bool hit) const;
    uint32_t defendSkillXpGain() const;
    uint32_t activitySkillXpGain(const std::string& activityId) const;

private:
    const content::SkillCapCatalog* skillCapCatalog_ = nullptr;
};

} // namespace tbeq
