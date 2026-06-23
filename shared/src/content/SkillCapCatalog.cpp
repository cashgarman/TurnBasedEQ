#include "tbeq/content/SkillCapCatalog.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

namespace
{

SkillCapRule parseRule(const nlohmann::json& json)
{
    SkillCapRule rule;
    rule.unlockAtLevel = json.value("unlockAtLevel", static_cast<uint16_t>(1));
    rule.capPerLevel = json.value("capPerLevel", static_cast<uint16_t>(0));
    rule.maxCap = json.value("maxCap", static_cast<uint16_t>(0));
    return rule;
}

} // namespace

bool SkillCapCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;

    classRules_.clear();
    if (json.contains("defaults"))
    {
        defaultRule_ = parseRule(json["defaults"]);
    }

    if (!json.contains("classes") || !json["classes"].is_object())
    {
        return false;
    }

    for (const auto& [classId, classJson] : json["classes"].items())
    {
        if (!classJson.is_object())
        {
            continue;
        }

        std::unordered_map<std::string, SkillCapRule> rules;
        for (const auto& [skillId, skillJson] : classJson.items())
        {
            rules[skillId] = parseRule(skillJson);
        }
        classRules_[classId] = std::move(rules);
    }

    return !classRules_.empty();
}

uint16_t SkillCapCatalog::getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const
{
    const auto classIt = classRules_.find(classId);
    if (classIt == classRules_.end())
    {
        return 0;
    }

    const auto skillIt = classIt->second.find(skillId);
    const SkillCapRule& rule = skillIt != classIt->second.end() ? skillIt->second : defaultRule_;
    if (rule.maxCap == 0 || characterLevel < rule.unlockAtLevel)
    {
        return 0;
    }

    const uint32_t rawCap = static_cast<uint32_t>(rule.capPerLevel) * static_cast<uint32_t>(characterLevel);
    return static_cast<uint16_t>(std::min(static_cast<uint32_t>(rule.maxCap), rawCap));
}

bool SkillCapCatalog::isSkillTrainable(
    const std::string& classId,
    const std::string& skillId,
    uint16_t characterLevel) const
{
    return getCap(classId, skillId, characterLevel) > 0;
}

} // namespace tbeq::content
