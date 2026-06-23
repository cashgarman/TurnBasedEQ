#include "tbeq/skills/SkillResolver.hpp"

#include <algorithm>

#include "tbeq/skills/Progression.hpp"

namespace tbeq
{

void SkillResolver::setSkillCapCatalog(const content::SkillCapCatalog* catalog)
{
    skillCapCatalog_ = catalog;
}

uint16_t SkillResolver::getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const
{
    if (skillCapCatalog_ != nullptr)
    {
        return skillCapCatalog_->getCap(classId, skillId, characterLevel);
    }
    return static_cast<uint16_t>(5 * characterLevel);
}

bool SkillResolver::canUseSkill(const SkillProgress& progress, uint16_t cap) const
{
    return cap > 0 && progress.level <= cap;
}

bool SkillResolver::applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap)
{
    if (cap == 0 || progress.level >= cap)
    {
        return false;
    }

    progress.experience += amount;
    bool changed = amount > 0;
    while (progress.level < cap)
    {
        const uint32_t required = progression::skillExperienceToLevelUp(progress.level);
        if (progress.experience < required)
        {
            break;
        }
        progress.experience -= required;
        ++progress.level;
        changed = true;
    }
    return changed;
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
    std::mt19937& rng,
    float damageMultiplier) const
{
    std::uniform_int_distribution<int32_t> dist(1, 6);
    const int32_t base = 8 + static_cast<int32_t>(attackerLevel)
        + static_cast<int32_t>(weaponSkill) / 10
        + static_cast<int32_t>(offenseSkill) / 15
        + dist(rng);
    const int32_t mitigation = static_cast<int32_t>(targetDefense) / 20;
    const int32_t scaled = static_cast<int32_t>(static_cast<float>(std::max(1, base - mitigation)) * damageMultiplier);
    return std::max(1, scaled);
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

bool SkillResolver::rollChanneling(uint16_t channelingSkill, std::mt19937& rng) const
{
    int32_t successChance = 40 + static_cast<int32_t>(channelingSkill) / 2;
    successChance = std::clamp(successChance, 15, 98);

    std::uniform_int_distribution<int32_t> dist(1, 100);
    return dist(rng) <= successChance;
}

bool SkillResolver::rollSpellResist(
    uint16_t casterSchoolSkill,
    uint16_t targetLevel,
    std::mt19937& rng) const
{
    int32_t resistChance = 20 + static_cast<int32_t>(targetLevel) * 2
        - static_cast<int32_t>(casterSchoolSkill) / 3;
    resistChance = std::clamp(resistChance, 5, 75);

    std::uniform_int_distribution<int32_t> dist(1, 100);
    return dist(rng) <= resistChance;
}

bool SkillResolver::rollManeuver(
    uint16_t maneuverSkill,
    uint16_t targetLevel,
    std::mt19937& rng) const
{
    int32_t successChance = 35 + static_cast<int32_t>(maneuverSkill) / 2
        - static_cast<int32_t>(targetLevel);
    successChance = std::clamp(successChance, 10, 90);

    std::uniform_int_distribution<int32_t> dist(1, 100);
    return dist(rng) <= successChance;
}

int32_t SkillResolver::calculateSpellDamage(
    int32_t baseValue,
    uint16_t schoolSkill,
    uint16_t casterLevel) const
{
    return baseValue + static_cast<int32_t>(schoolSkill) / 5 + static_cast<int32_t>(casterLevel) / 2;
}

int32_t SkillResolver::calculateHealAmount(
    int32_t baseValue,
    uint16_t schoolSkill,
    uint16_t casterLevel) const
{
    return baseValue + static_cast<int32_t>(schoolSkill) / 4 + static_cast<int32_t>(casterLevel);
}

int32_t SkillResolver::calculateAbilityDamage(
    int32_t baseValue,
    uint16_t linkedSkillLevel,
    uint16_t offenseSkill,
    uint16_t attackerLevel) const
{
    return baseValue + static_cast<int32_t>(linkedSkillLevel) / 4
        + static_cast<int32_t>(offenseSkill) / 8
        + static_cast<int32_t>(attackerLevel) / 2;
}

uint16_t SkillResolver::meditateManaGain(uint16_t meditateSkill, uint16_t maxMana) const
{
    const uint16_t baseGain = static_cast<uint16_t>(5 + meditateSkill / 10);
    return std::min(maxMana, baseGain);
}

uint32_t SkillResolver::combatSkillXpGain(bool hit) const
{
    return hit ? 8u : 4u;
}

uint32_t SkillResolver::defendSkillXpGain() const
{
    return 6u;
}

uint32_t SkillResolver::activitySkillXpGain(const std::string& activityId) const
{
    if (activityId == "forage")
    {
        return 10u;
    }
    if (activityId == "pick_lock")
    {
        return 12u;
    }
    if (activityId == "meditate")
    {
        return 5u;
    }
    return 6u;
}

} // namespace tbeq
