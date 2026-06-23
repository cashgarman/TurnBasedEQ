#include "tbeq/skills/SkillResolver.hpp"

#include <algorithm>

namespace tbeq
{

uint16_t SkillResolver::getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const
{
    (void)classId;
    (void)skillId;
    return static_cast<uint16_t>(5 * characterLevel);
}

bool SkillResolver::canUseSkill(const SkillProgress& progress, uint16_t cap) const
{
    return progress.level <= cap;
}

void SkillResolver::applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap)
{
    if (progress.level >= cap)
    {
        return;
    }
    progress.experience += amount;
    while (progress.experience >= 100 && progress.level < cap)
    {
        progress.experience -= 100;
        ++progress.level;
    }
}

bool SkillResolver::rollMeleeHit(
    uint16_t offenseSkill,
    uint16_t weaponSkill,
    uint16_t targetDefense,
    std::mt19937& rng) const
{
    const int32_t attackScore = static_cast<int32_t>(offenseSkill) + static_cast<int32_t>(weaponSkill);
    const int32_t defenseScore = static_cast<int32_t>(targetDefense);
    int32_t hitChance = 50 + (attackScore - defenseScore) / 2;
    hitChance = std::clamp(hitChance, 5, 95);

    std::uniform_int_distribution<int32_t> dist(1, 100);
    return dist(rng) <= hitChance;
}

int32_t SkillResolver::calculateMeleeDamage(
    uint16_t attackerLevel,
    uint16_t weaponSkill,
    uint16_t offenseSkill,
    uint16_t targetDefense,
    std::mt19937& rng) const
{
    std::uniform_int_distribution<int32_t> dist(1, 6);
    const int32_t base = 8 + static_cast<int32_t>(attackerLevel)
        + static_cast<int32_t>(weaponSkill) / 10
        + static_cast<int32_t>(offenseSkill) / 15
        + dist(rng);
    const int32_t mitigation = static_cast<int32_t>(targetDefense) / 20;
    return std::max(1, base - mitigation);
}

bool SkillResolver::rollFlee(
    uint16_t defenseSkill,
    uint16_t agility,
    int32_t livingEnemyCount,
    std::mt19937& rng) const
{
    int32_t fleeChance = 30 + static_cast<int32_t>(defenseSkill) / 5 + static_cast<int32_t>(agility) / 10
        - livingEnemyCount * 5;
    fleeChance = std::clamp(fleeChance, 10, 90);

    std::uniform_int_distribution<int32_t> dist(1, 100);
    return dist(rng) <= fleeChance;
}

uint32_t SkillResolver::combatSkillXpGain(bool hit) const
{
    return hit ? 8u : 4u;
}

} // namespace tbeq
