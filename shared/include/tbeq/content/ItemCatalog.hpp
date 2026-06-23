#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::content
{

enum class ItemSlot : uint8_t
{
    None = 0,
    Weapon = 1,
    Head = 2,
    Chest = 3,
    Hands = 4,
};

enum class ItemRarity : uint8_t
{
    Common = 0,
    Uncommon = 1,
    Rare = 2,
};

struct ItemStatBlock
{
    int16_t hp = 0;
    int16_t mana = 0;
    int16_t offense = 0;
    int16_t defense = 0;
};

struct ItemDef
{
    std::string id;
    std::string name;
    uint16_t stackSize = 1;
    ItemSlot slot = ItemSlot::None;
    ItemRarity rarity = ItemRarity::Common;
    uint32_t vendorValue = 1;
    ItemStatBlock stats;
    uint16_t requiredLevel = 1;
    std::vector<std::string> allowedClasses;
    std::vector<std::string> allowedRaces;
    std::string requiredSkillId;
    uint16_t requiredSkillLevel = 0;
    std::string weaponSkill;
    std::string spriteTint;
    std::string iconShape = "square";
};

class ItemCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);
    const ItemDef* findItem(const std::string& itemId) const;

private:
    std::unordered_map<std::string, ItemDef> items_;
};

ItemSlot parseItemSlot(const std::string& value);
std::string itemSlotToString(ItemSlot slot);
ItemRarity parseItemRarity(const std::string& value);

} // namespace tbeq::content
