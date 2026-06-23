#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace tbeq::content
{

struct SkillCapRule
{
    uint16_t unlockAtLevel = 1;
    uint16_t capPerLevel = 0;
    uint16_t maxCap = 0;
};

class SkillCapCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    uint16_t getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const;
    bool isSkillTrainable(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const;

private:
    SkillCapRule defaultRule_;
    std::unordered_map<std::string, std::unordered_map<std::string, SkillCapRule>> classRules_;
};

} // namespace tbeq::content
