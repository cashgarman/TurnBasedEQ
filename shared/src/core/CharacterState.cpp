#include "tbeq/core/CharacterState.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace tbeq
{

namespace
{

void applyClassSkills(CharacterState& state, const std::string& classId, uint16_t level)
{
    const uint16_t baseSkill = static_cast<uint16_t>(5 + level);
    state.skills["offense"] = {baseSkill, 0};
    state.skills["defense"] = {baseSkill, 0};
    state.skills["merchant"] = {baseSkill, 0};

    if (classId == "warrior")
    {
        state.skills["1h_slash"] = {baseSkill, 0};
        state.skills["bash"] = {baseSkill, 0};
        state.skills["kick"] = {baseSkill, 0};
        state.skills["taunt"] = {static_cast<uint16_t>(std::max<uint16_t>(1, baseSkill - 2)), 0};
        state.skills["block"] = {baseSkill, 0};
        state.equippedWeaponSkill = "1h_slash";
        state.unlockedAbilities = {"warrior_bash", "warrior_kick", "warrior_taunt"};
    }
    else if (classId == "cleric")
    {
        state.skills["1h_blunt"] = {baseSkill, 0};
        state.skills["alteration"] = {baseSkill, 0};
        state.skills["evocation"] = {static_cast<uint16_t>(baseSkill / 2), 0};
        state.skills["channeling"] = {baseSkill, 0};
        state.skills["meditate"] = {baseSkill, 0};
        state.equippedWeaponSkill = "1h_blunt";
        state.unlockedSpells = {"cleric_heal"};
    }
    else if (classId == "wizard")
    {
        state.skills["evocation"] = {baseSkill, 0};
        state.skills["abjuration"] = {static_cast<uint16_t>(baseSkill / 2), 0};
        state.skills["channeling"] = {baseSkill, 0};
        state.skills["meditate"] = {baseSkill, 0};
        state.equippedWeaponSkill = "1h_blunt";
        state.unlockedSpells = {"wizard_nuke"};
    }
    else if (classId == "rogue")
    {
        state.skills["1h_pierce"] = {baseSkill, 0};
        state.skills["hide"] = {baseSkill, 0};
        state.skills["sneak"] = {baseSkill, 0};
        state.skills["pick_lock"] = {static_cast<uint16_t>(baseSkill / 2), 0};
        state.equippedWeaponSkill = "1h_pierce";
        state.unlockedAbilities = {"rogue_backstab"};
    }
    else
    {
        state.skills["1h_slash"] = {baseSkill, 0};
        state.equippedWeaponSkill = "1h_slash";
    }
}

} // namespace

CharacterState CharacterState::createDefault(const std::string& classId, uint16_t level)
{
    CharacterState state;
    state.classId = classId;
    state.level = level;
    state.maxHp = static_cast<uint16_t>(80 + level * 10);
    state.hp = state.maxHp;
    state.agi = static_cast<uint16_t>(70 + level);
    state.cha = 100;
    state.gold = 200;

    if (classId == "warrior" || classId == "rogue")
    {
        state.maxMana = 0;
        state.mana = 0;
    }
    else
    {
        state.maxMana = static_cast<uint16_t>(40 + level * 8);
        state.mana = state.maxMana;
    }

    applyClassSkills(state, classId, level);
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
        if (parsed.contains("classId"))
        {
            state.classId = parsed["classId"].get<std::string>();
        }
        if (parsed.contains("level"))
        {
            state.level = parsed["level"].get<uint16_t>();
        }
        if (parsed.contains("experience"))
        {
            state.experience = parsed["experience"].get<uint32_t>();
        }
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
        if (parsed.contains("cha"))
        {
            state.cha = parsed["cha"].get<uint16_t>();
        }
        if (parsed.contains("gold"))
        {
            state.gold = parsed["gold"].get<uint32_t>();
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
        if (parsed.contains("unlockedSpells") && parsed["unlockedSpells"].is_array())
        {
            state.unlockedSpells.clear();
            for (const auto& spellId : parsed["unlockedSpells"])
            {
                state.unlockedSpells.push_back(spellId.get<std::string>());
            }
        }
        if (parsed.contains("unlockedAbilities") && parsed["unlockedAbilities"].is_array())
        {
            state.unlockedAbilities.clear();
            for (const auto& abilityId : parsed["unlockedAbilities"])
            {
                state.unlockedAbilities.push_back(abilityId.get<std::string>());
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
        if (parsed.contains("equipment") && parsed["equipment"].is_object())
        {
            state.equipment.clear();
            for (const auto& [slot, itemId] : parsed["equipment"].items())
            {
                state.equipment[slot] = itemId.get<std::string>();
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
    parsed["classId"] = classId;
    parsed["level"] = level;
    parsed["experience"] = experience;
    parsed["hp"] = hp;
    parsed["maxHp"] = maxHp;
    parsed["mana"] = mana;
    parsed["maxMana"] = maxMana;
    parsed["agi"] = agi;
    parsed["cha"] = cha;
    parsed["gold"] = gold;
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

    nlohmann::json spellsJson = nlohmann::json::array();
    for (const auto& spellId : unlockedSpells)
    {
        spellsJson.push_back(spellId);
    }
    parsed["unlockedSpells"] = spellsJson;

    nlohmann::json abilitiesJson = nlohmann::json::array();
    for (const auto& abilityId : unlockedAbilities)
    {
        abilitiesJson.push_back(abilityId);
    }
    parsed["unlockedAbilities"] = abilitiesJson;

    nlohmann::json inventoryJson = nlohmann::json::array();
    for (const auto& item : inventory)
    {
        inventoryJson.push_back({
            {"itemId", item.itemId},
            {"quantity", item.quantity}});
    }
    parsed["inventory"] = inventoryJson;

    nlohmann::json equipmentJson = nlohmann::json::object();
    for (const auto& [slot, itemId] : equipment)
    {
        equipmentJson[slot] = itemId;
    }
    parsed["equipment"] = equipmentJson;
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

bool CharacterState::removeItem(const std::string& itemId, uint16_t quantity)
{
    for (auto it = inventory.begin(); it != inventory.end(); ++it)
    {
        if (it->itemId != itemId)
        {
            continue;
        }
        if (it->quantity < quantity)
        {
            return false;
        }
        it->quantity = static_cast<uint16_t>(it->quantity - quantity);
        if (it->quantity == 0)
        {
            inventory.erase(it);
        }
        return true;
    }
    return false;
}

uint16_t CharacterState::itemQuantity(const std::string& itemId) const
{
    for (const auto& item : inventory)
    {
        if (item.itemId == itemId)
        {
            return item.quantity;
        }
    }
    return 0;
}

std::string CharacterState::equippedItemInSlot(const std::string& slot) const
{
    const auto it = equipment.find(slot);
    if (it == equipment.end())
    {
        return {};
    }
    return it->second;
}

bool CharacterState::knowsSpell(const std::string& spellId) const
{
    return std::find(unlockedSpells.begin(), unlockedSpells.end(), spellId) != unlockedSpells.end();
}

bool CharacterState::knowsAbility(const std::string& abilityId) const
{
    return std::find(unlockedAbilities.begin(), unlockedAbilities.end(), abilityId) != unlockedAbilities.end();
}

void CharacterState::unlockAllClassContent(
    const std::vector<std::string>& spellIds,
    const std::vector<std::string>& abilityIds)
{
    unlockedSpells = spellIds;
    unlockedAbilities = abilityIds;
}

} // namespace tbeq
