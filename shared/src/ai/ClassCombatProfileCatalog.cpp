#include "tbeq/ai/ClassCombatProfile.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::ai
{

bool ClassCombatProfileCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input)
    {
        return false;
    }

    nlohmann::json root;
    input >> root;
    if (!root.is_object())
    {
        return false;
    }

    profiles_.clear();
    for (const auto& [classId, entry] : root.items())
    {
        ClassCombatProfile profile;
        profile.healThreshold = entry.value("healThreshold", 0.0f);
        profile.nukeThreshold = entry.value("nukeThreshold", 0.0f);
        profile.fleeHpPercent = entry.value("fleeHpPercent", 0.15f);
        profile.preferredHealSpell = entry.value("preferredHealSpell", std::string{});
        profile.preferredNukeSpell = entry.value("preferredNukeSpell", std::string{});
        profile.preferredAbility = entry.value("preferredAbility", std::string{});
        profiles_.emplace(classId, std::move(profile));
    }

    return true;
}

const ClassCombatProfile* ClassCombatProfileCatalog::findProfile(const std::string& classId) const
{
    const auto it = profiles_.find(classId);
    if (it == profiles_.end())
    {
        return nullptr;
    }
    return &it->second;
}

} // namespace tbeq::ai
