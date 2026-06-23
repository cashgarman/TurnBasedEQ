#include "tbeq/content/SpellCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

namespace
{

SpellTargetType parseTargetType(const std::string& value)
{
    if (value == "single_ally")
    {
        return SpellTargetType::SingleAlly;
    }
    if (value == "self")
    {
        return SpellTargetType::Self;
    }
    return SpellTargetType::SingleEnemy;
}

SpellEffectType parseEffectType(const std::string& value)
{
    if (value == "heal")
    {
        return SpellEffectType::Heal;
    }
    if (value == "dot")
    {
        return SpellEffectType::Dot;
    }
    return SpellEffectType::Damage;
}

} // namespace

bool SpellCatalog::loadFromFile(const std::filesystem::path& path)
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

    spells_.clear();
    for (const auto& entry : root)
    {
        SpellDef def;
        def.id = entry.value("id", std::string{});
        def.name = entry.value("name", def.id);
        def.classId = entry.value("classId", std::string{});
        def.manaCost = entry.value("manaCost", static_cast<uint16_t>(0));
        def.school = entry.value("school", std::string{"evocation"});
        def.targetType = parseTargetType(entry.value("targetType", std::string{"single_enemy"}));
        def.effect = parseEffectType(entry.value("effect", std::string{"damage"}));
        def.baseValue = entry.value("baseValue", static_cast<int32_t>(0));
        def.linkedSkill = entry.value("linkedSkill", def.school);
        def.minLevel = entry.value("minLevel", static_cast<uint16_t>(1));
        def.dotTurns = entry.value("dotTurns", static_cast<uint16_t>(0));
        if (!def.id.empty())
        {
            spells_.emplace(def.id, std::move(def));
        }
    }

    return true;
}

const SpellDef* SpellCatalog::findSpell(const std::string& spellId) const
{
    const auto it = spells_.find(spellId);
    if (it == spells_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<const SpellDef*> SpellCatalog::spellsForClass(const std::string& classId, uint16_t level) const
{
    std::vector<const SpellDef*> result;
    for (const auto& [_, def] : spells_)
    {
        if (def.classId == classId && def.minLevel <= level)
        {
            result.push_back(&def);
        }
    }
    return result;
}

} // namespace tbeq::content
