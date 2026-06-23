#include "tbeq/core/CharacterState.hpp"

#include <nlohmann/json.hpp>

namespace tbeq
{

CharacterState CharacterState::createDefault(const std::string& classId, uint16_t level)
{
    (void)classId;
    CharacterState state;
    state.maxHp = static_cast<uint16_t>(80 + level * 10);
    state.hp = state.maxHp;
    state.maxMana = static_cast<uint16_t>(40 + level * 5);
    state.mana = state.maxMana;
    state.agi = static_cast<uint16_t>(70 + level);

    const uint16_t baseSkill = static_cast<uint16_t>(5 + level);
    state.skills["offense"] = {baseSkill, 0};
    state.skills["defense"] = {baseSkill, 0};
    state.skills["1h_slash"] = {baseSkill, 0};
    return state;
}

CharacterState CharacterState::fromJson(const std::string& json)
{
    CharacterState state = createDefault("warrior", 1);
    if (json.empty() || json == "{}")
    {
        return state;
    }

    try
    {
        const auto parsed = nlohmann::json::parse(json);
        if (parsed.contains("hp"))
        {
            state.hp = parsed["hp"].get<uint16_t>();
        }
        if (parsed.contains("maxHp"))
        {
            state.maxHp = parsed["maxHp"].get<uint16_t>();
        }
        if (parsed.contains("mana"))
        {
            state.mana = parsed["mana"].get<uint16_t>();
        }
        if (parsed.contains("maxMana"))
        {
            state.maxMana = parsed["maxMana"].get<uint16_t>();
        }
        if (parsed.contains("agi"))
        {
            state.agi = parsed["agi"].get<uint16_t>();
        }
        if (parsed.contains("equippedWeaponSkill"))
        {
            state.equippedWeaponSkill = parsed["equippedWeaponSkill"].get<std::string>();
        }
        if (parsed.contains("godMode"))
        {
            state.godMode = parsed["godMode"].get<bool>();
        }
        if (parsed.contains("skills") && parsed["skills"].is_object())
        {
            state.skills.clear();
            for (const auto& [skillId, skillJson] : parsed["skills"].items())
            {
                SkillProgress progress;
                progress.level = skillJson.value("level", static_cast<uint16_t>(1));
                progress.experience = skillJson.value("experience", static_cast<uint32_t>(0));
                state.skills[skillId] = progress;
            }
        }
        if (parsed.contains("inventory") && parsed["inventory"].is_array())
        {
            state.inventory.clear();
            for (const auto& itemJson : parsed["inventory"])
            {
                InventoryItem item;
                item.itemId = itemJson.value("itemId", std::string{});
                item.quantity = itemJson.value("quantity", static_cast<uint16_t>(1));
                if (!item.itemId.empty())
                {
                    state.inventory.push_back(std::move(item));
                }
            }
        }
    }
    catch (const std::exception&)
    {
        return createDefault("warrior", 1);
    }

    return state;
}

std::string CharacterState::toJson() const
{
    nlohmann::json parsed;
    parsed["hp"] = hp;
    parsed["maxHp"] = maxHp;
    parsed["mana"] = mana;
    parsed["maxMana"] = maxMana;
    parsed["agi"] = agi;
    parsed["equippedWeaponSkill"] = equippedWeaponSkill;
    parsed["godMode"] = godMode;

    nlohmann::json skillsJson = nlohmann::json::object();
    for (const auto& [skillId, progress] : skills)
    {
        skillsJson[skillId] = {
            {"level", progress.level},
            {"experience", progress.experience}};
    }
    parsed["skills"] = skillsJson;

    nlohmann::json inventoryJson = nlohmann::json::array();
    for (const auto& item : inventory)
    {
        inventoryJson.push_back({
            {"itemId", item.itemId},
            {"quantity", item.quantity}});
    }
    parsed["inventory"] = inventoryJson;
    return parsed.dump();
}

SkillProgress& CharacterState::skillOrDefault(const std::string& skillId)
{
    return skills[skillId];
}

uint16_t CharacterState::skillLevel(const std::string& skillId) const
{
    const auto it = skills.find(skillId);
    if (it == skills.end())
    {
        return 1;
    }
    return it->second.level;
}

void CharacterState::addItem(const std::string& itemId, uint16_t quantity)
{
    for (auto& item : inventory)
    {
        if (item.itemId == itemId)
        {
            item.quantity = static_cast<uint16_t>(item.quantity + quantity);
            return;
        }
    }
    inventory.push_back({itemId, quantity});
}

} // namespace tbeq
