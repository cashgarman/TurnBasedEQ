#pragma once

#include <cstdint>

#include "tbeq/core/CharacterState.hpp"

namespace tbeq::progression
{

uint32_t experienceRequiredForLevel(uint16_t level);
uint32_t skillExperienceToLevelUp(uint16_t skillLevel);

struct CharacterLevelUpResult
{
    bool leveledUp = false;
    uint16_t oldLevel = 1;
    uint16_t newLevel = 1;
    uint32_t experienceGranted = 0;
};

CharacterLevelUpResult grantCharacterExperience(CharacterState& state, uint32_t amount);
void applyCharacterLevelUp(CharacterState& state);

uint32_t mobExperienceReward(uint16_t mobLevel, uint16_t mobHp, bool isNamed, bool isBoss);

} // namespace tbeq::progression
