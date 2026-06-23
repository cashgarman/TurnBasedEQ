#include "tbeq/skills/Progression.hpp"

#include <algorithm>

namespace tbeq::progression
{

uint32_t experienceRequiredForLevel(uint16_t level)
{
    if (level <= 1)
    {
        return 0;
    }
    return static_cast<uint32_t>(level) * static_cast<uint32_t>(level) * 50u;
}

uint32_t skillExperienceToLevelUp(uint16_t skillLevel)
{
    return 50u + static_cast<uint32_t>(skillLevel) * 25u;
}

void applyCharacterLevelUp(CharacterState& state)
{
    state.maxHp = static_cast<uint16_t>(80 + state.level * 10);
    if (state.maxMana > 0 || state.classId == "cleric" || state.classId == "wizard")
    {
        state.maxMana = static_cast<uint16_t>(40 + state.level * 8);
    }
    state.hp = state.maxHp;
    if (state.maxMana > 0)
    {
        state.mana = state.maxMana;
    }
    state.agi = static_cast<uint16_t>(70 + state.level);
}

CharacterLevelUpResult grantCharacterExperience(CharacterState& state, uint32_t amount)
{
    CharacterLevelUpResult result;
    result.oldLevel = state.level;
    result.newLevel = state.level;
    result.experienceGranted = amount;

    if (amount == 0)
    {
        return result;
    }

    state.experience += amount;
    while (state.level < 60)
    {
        const uint32_t required = experienceRequiredForLevel(static_cast<uint16_t>(state.level + 1));
        if (state.experience < required)
        {
            break;
        }
        state.experience -= required;
        ++state.level;
        applyCharacterLevelUp(state);
        result.leveledUp = true;
        result.newLevel = state.level;
    }

    return result;
}

uint32_t mobExperienceReward(uint16_t mobLevel, uint16_t mobHp, bool isNamed, bool isBoss)
{
    uint32_t reward = static_cast<uint32_t>(mobLevel) * 25u + static_cast<uint32_t>(mobHp) / 2u;
    if (isNamed)
    {
        reward = static_cast<uint32_t>(reward * 1.5f);
    }
    if (isBoss)
    {
        reward *= 3u;
    }
    return std::max(1u, reward);
}

} // namespace tbeq::progression
