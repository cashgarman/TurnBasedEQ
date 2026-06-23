#pragma once

#include <cstdint>
#include <string>

namespace tbeq
{

enum class SkillCategory : uint8_t
{
    MeleeWeapon,
    CombatFundamentals,
    CombatManeuvers,
    Casting,
    StealthUtility,
    Crafting,
    Trade,
};

struct SkillDef
{
    std::string id;
    SkillCategory category = SkillCategory::MeleeWeapon;
    std::string displayName;
    std::string description;
};

} // namespace tbeq
