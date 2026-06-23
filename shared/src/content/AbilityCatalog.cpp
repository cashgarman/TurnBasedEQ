#include "tbeq/content/AbilityCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

namespace
{

AbilityTargetType parseTargetType(const std::string& value)
{
    if (value == "self")
    {
        return AbilityTargetType::Self;
    }
    return AbilityTargetType::SingleEnemy;
}

AbilityEffectType parseEffectType(const std::string& value)
{
    if (value == "damage_stun")
    {
        return AbilityEffectType::DamageStun;
    }
    if (value == "damage_snare")
    {
        return AbilityEffectType::DamageSnare;
    }
    if (value == "taunt")
    {
        return AbilityEffectType::Taunt;
    }
    return AbilityEffectType::Damage;
}

} // namespace

bool AbilityCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input)
    {
        return false;
    }

    nlohmann::json root;
    input >> root;
    if (!root.is_array())
    {
        return false;
    }

    abilities_.clear();
    for (const auto& entry : root)
    {
        AbilityDef def;
        def.id = entry.value("id", std::string{});
        def.name = entry.value("name", def.id);
        def.classId = entry.value("classId", std::string{});
        def.targetType = parseTargetType(entry.value("targetType", std::string{"single_enemy"}));
        def.effect = parseEffectType(entry.value("effect", std::string{"damage"}));
        def.baseValue = entry.value("baseValue", static_cast<int32_t>(0));
        def.linkedSkill = entry.value("linkedSkill", std::string{"offense"});
        def.skillCheck = entry.value("skillCheck", def.linkedSkill);
        def.minLevel = entry.value("minLevel", static_cast<uint16_t>(1));
        def.stunTurns = entry.value("stunTurns", static_cast<uint16_t>(0));
        def.snareTurns = entry.value("snareTurns", static_cast<uint16_t>(0));
        if (!def.id.empty())
        {
            abilities_.emplace(def.id, std::move(def));
        }
    }

    return true;
}

const AbilityDef* AbilityCatalog::findAbility(const std::string& abilityId) const
{
    const auto it = abilities_.find(abilityId);
    if (it == abilities_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<const AbilityDef*> AbilityCatalog::abilitiesForClass(const std::string& classId, uint16_t level) const
{
    std::vector<const AbilityDef*> result;
    for (const auto& [_, def] : abilities_)
    {
        if (def.classId == classId && def.minLevel <= level)
        {
            result.push_back(&def);
        }
    }
    return result;
}

} // namespace tbeq::content
