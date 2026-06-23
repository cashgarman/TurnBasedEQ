#include "tbeq/content/ItemCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

ItemSlot parseItemSlot(const std::string& value)
{
    if (value == "weapon")
    {
        return ItemSlot::Weapon;
    }
    if (value == "head")
    {
        return ItemSlot::Head;
    }
    if (value == "chest")
    {
        return ItemSlot::Chest;
    }
    if (value == "hands")
    {
        return ItemSlot::Hands;
    }
    return ItemSlot::None;
}

std::string itemSlotToString(ItemSlot slot)
{
    switch (slot)
    {
    case ItemSlot::Weapon:
        return "weapon";
    case ItemSlot::Head:
        return "head";
    case ItemSlot::Chest:
        return "chest";
    case ItemSlot::Hands:
        return "hands";
    default:
        return "";
    }
}

ItemRarity parseItemRarity(const std::string& value)
{
    if (value == "uncommon")
    {
        return ItemRarity::Uncommon;
    }
    if (value == "rare")
    {
        return ItemRarity::Rare;
    }
    return ItemRarity::Common;
}

bool ItemCatalog::loadFromFile(const std::filesystem::path& path)
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

    items_.clear();
    for (const auto& entry : root)
    {
        ItemDef def;
        def.id = entry.value("id", std::string{});
        def.name = entry.value("name", def.id);
        def.stackSize = entry.value("stackSize", static_cast<uint16_t>(1));
        def.slot = parseItemSlot(entry.value("slot", std::string{}));
        def.rarity = parseItemRarity(entry.value("rarity", std::string{"common"}));
        def.vendorValue = entry.value("vendorValue", static_cast<uint32_t>(1));
        def.requiredLevel = entry.value("requiredLevel", static_cast<uint16_t>(1));
        def.weaponSkill = entry.value("weaponSkill", std::string{});
        def.spriteTint = entry.value("spriteTint", std::string{});
        def.iconShape = entry.value("iconShape", std::string{"square"});

        if (entry.contains("stats") && entry["stats"].is_object())
        {
            const auto& stats = entry["stats"];
            def.stats.hp = stats.value("hp", static_cast<int16_t>(0));
            def.stats.mana = stats.value("mana", static_cast<int16_t>(0));
            def.stats.offense = stats.value("offense", static_cast<int16_t>(0));
            def.stats.defense = stats.value("defense", static_cast<int16_t>(0));
        }

        if (entry.contains("allowedClasses") && entry["allowedClasses"].is_array())
        {
            for (const auto& classId : entry["allowedClasses"])
            {
                def.allowedClasses.push_back(classId.get<std::string>());
            }
        }
        if (entry.contains("allowedRaces") && entry["allowedRaces"].is_array())
        {
            for (const auto& raceId : entry["allowedRaces"])
            {
                def.allowedRaces.push_back(raceId.get<std::string>());
            }
        }
        if (entry.contains("requiredSkill") && entry["requiredSkill"].is_object())
        {
            def.requiredSkillId = entry["requiredSkill"].value("id", std::string{});
            def.requiredSkillLevel = entry["requiredSkill"].value("level", static_cast<uint16_t>(0));
        }

        if (!def.id.empty())
        {
            items_.emplace(def.id, std::move(def));
        }
    }

    return true;
}

const ItemDef* ItemCatalog::findItem(const std::string& itemId) const
{
    const auto it = items_.find(itemId);
    if (it == items_.end())
    {
        return nullptr;
    }
    return &it->second;
}

} // namespace tbeq::content
