#pragma once

#include <cstdint>
#include <string>

#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"

namespace tbeq::items
{

struct EquipContext
{
    std::string classId;
    std::string raceId;
    uint16_t level = 1;
};

struct EquipCheckResult
{
    bool ok = false;
    std::string message;
};

class ItemRules
{
public:
    static EquipCheckResult canEquip(
        const content::ItemDef& item,
        const EquipContext& context,
        const CharacterState& state);

    static void recomputeDerivedStats(CharacterState& state, const content::ItemCatalog& catalog);

    static uint32_t merchantBuyPrice(
        const content::ItemDef& item,
        uint16_t merchantSkill,
        uint16_t charisma,
        uint32_t listedPrice = 0);

    static uint32_t merchantSellPrice(
        const content::ItemDef& item,
        uint16_t merchantSkill,
        uint16_t charisma);
};

} // namespace tbeq::items
