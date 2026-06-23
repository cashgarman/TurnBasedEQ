#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "tbeq/skills/SkillDef.hpp"

namespace tbeq::content
{

class SkillCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    const SkillDef* findSkill(const std::string& skillId) const;
    std::vector<const SkillDef*> allSkills() const;
    std::vector<const SkillDef*> skillsInCategory(SkillCategory category) const;

private:
    std::unordered_map<std::string, SkillDef> skills_;
};

SkillCategory parseSkillCategory(const std::string& value);
std::string skillCategoryToString(SkillCategory category);

} // namespace tbeq::content
