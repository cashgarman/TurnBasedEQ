#include "tbeq/content/SkillCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

SkillCategory parseSkillCategory(const std::string& value)
{
    if (value == "melee_weapon")
    {
        return SkillCategory::MeleeWeapon;
    }
    if (value == "combat_fundamentals")
    {
        return SkillCategory::CombatFundamentals;
    }
    if (value == "combat_maneuvers")
    {
        return SkillCategory::CombatManeuvers;
    }
    if (value == "casting")
    {
        return SkillCategory::Casting;
    }
    if (value == "casting_support")
    {
        return SkillCategory::CastingSupport;
    }
    if (value == "stealth_utility")
    {
        return SkillCategory::StealthUtility;
    }
    if (value == "exploration")
    {
        return SkillCategory::Exploration;
    }
    if (value == "support")
    {
        return SkillCategory::Support;
    }
    if (value == "crafting")
    {
        return SkillCategory::Crafting;
    }
    if (value == "trade")
    {
        return SkillCategory::Trade;
    }
    return SkillCategory::MeleeWeapon;
}

std::string skillCategoryToString(SkillCategory category)
{
    switch (category)
    {
    case SkillCategory::MeleeWeapon:
        return "melee_weapon";
    case SkillCategory::CombatFundamentals:
        return "combat_fundamentals";
    case SkillCategory::CombatManeuvers:
        return "combat_maneuvers";
    case SkillCategory::Casting:
        return "casting";
    case SkillCategory::CastingSupport:
        return "casting_support";
    case SkillCategory::StealthUtility:
        return "stealth_utility";
    case SkillCategory::Exploration:
        return "exploration";
    case SkillCategory::Support:
        return "support";
    case SkillCategory::Crafting:
        return "crafting";
    case SkillCategory::Trade:
        return "trade";
    default:
        return "melee_weapon";
    }
}

bool SkillCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_array())
    {
        return false;
    }

    skills_.clear();
    for (const auto& entry : json)
    {
        SkillDef skill;
        skill.id = entry.value("id", std::string{});
        skill.displayName = entry.value("displayName", skill.id);
        skill.description = entry.value("description", std::string{});
        skill.category = parseSkillCategory(entry.value("category", std::string{"melee_weapon"}));
        if (!skill.id.empty())
        {
            skills_[skill.id] = std::move(skill);
        }
    }

    return !skills_.empty();
}

const SkillDef* SkillCatalog::findSkill(const std::string& skillId) const
{
    const auto it = skills_.find(skillId);
    if (it == skills_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<const SkillDef*> SkillCatalog::allSkills() const
{
    std::vector<const SkillDef*> skills;
    skills.reserve(skills_.size());
    for (const auto& [_, skill] : skills_)
    {
        skills.push_back(&skill);
    }
    return skills;
}

std::vector<const SkillDef*> SkillCatalog::skillsInCategory(SkillCategory category) const
{
    std::vector<const SkillDef*> skills;
    for (const auto& [_, skill] : skills_)
    {
        if (skill.category == category)
        {
            skills.push_back(&skill);
        }
    }
    return skills;
}

} // namespace tbeq::content
