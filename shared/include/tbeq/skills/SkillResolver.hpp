#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

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
    uint16_t getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const;
    bool canUseSkill(const SkillProgress& progress, uint16_t cap) const;
    void applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap);

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
        std::mt19937& rng) const;

    bool rollFlee(
        uint16_t defenseSkill,
        uint16_t agility,
        int32_t livingEnemyCount,
        std::mt19937& rng) const;

    uint32_t combatSkillXpGain(bool hit) const;

private:
    std::unordered_map<std::string, uint16_t> defaultCaps_;
};

} // namespace tbeq
