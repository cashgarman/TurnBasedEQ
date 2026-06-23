#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace tbeq::ai
{

struct ClassCombatProfile
{
    float healThreshold = 0.0f;
    float nukeThreshold = 0.0f;
    float fleeHpPercent = 0.15f;
    std::string preferredHealSpell;
    std::string preferredNukeSpell;
    std::string preferredAbility;
};

class ClassCombatProfileCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);
    const ClassCombatProfile* findProfile(const std::string& classId) const;

private:
    std::unordered_map<std::string, ClassCombatProfile> profiles_;
};

} // namespace tbeq::ai
