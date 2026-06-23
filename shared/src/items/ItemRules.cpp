#include "tbeq/items/ItemRules.hpp"

#include <algorithm>

namespace tbeq::items
{

namespace
{

bool containsId(const std::vector<std::string>& values, const std::string& id)
{
    return std::find(values.begin(), values.end(), id) != values.end();
}

float merchantPriceModifier(uint16_t merchantSkill, uint16_t charisma)
{
    float modifier = 1.0f
        - static_cast<float>(merchantSkill) / 300.0f * 0.20f
        - static_cast<float>(charisma) / 255.0f * 0.10f;
    return std::clamp(modifier, 0.50f, 1.0f);
}

} // namespace

EquipCheckResult ItemRules::canEquip(
    const content::ItemDef& item,
    const EquipContext& context,
    const CharacterState& state)
{
    EquipCheckResult result;
    if (item.slot == content::ItemSlot::None)
    {
        result.message = "Item is not equippable.";
        return result;
    }
    if (context.level < item.requiredLevel)
    {
        result.message = "Level too low.";
        return result;
    }
    if (!item.allowedClasses.empty() && !containsId(item.allowedClasses, context.classId))
    {
        result.message = "Class cannot equip this item.";
        return result;
    }
    if (!item.allowedRaces.empty() && !containsId(item.allowedRaces, context.raceId))
    {
        result.message = "Race cannot equip this item.";
        return result;
    }
    if (!item.requiredSkillId.empty() && state.skillLevel(item.requiredSkillId) < item.requiredSkillLevel)
    {
        result.message = "Skill requirement not met.";
        return result;
    }

    result.ok = true;
    result.message = "OK";
    return result;
}

void ItemRules::recomputeDerivedStats(CharacterState& state, const content::ItemCatalog& catalog)
{
    CharacterState baseline = CharacterState::createDefault(state.classId, state.level);
    state.maxHp = baseline.maxHp;
    state.maxMana = baseline.maxMana;

    for (const auto& [slot, itemId] : state.equipment)
    {
        (void)slot;
        const content::ItemDef* item = catalog.findItem(itemId);
        if (item == nullptr)
        {
            continue;
        }

        state.maxHp = static_cast<uint16_t>(std::max(
            0,
            static_cast<int32_t>(state.maxHp) + item->stats.hp));
        state.maxMana = static_cast<uint16_t>(std::max(
            0,
            static_cast<int32_t>(state.maxMana) + item->stats.mana));

        if (item->slot == content::ItemSlot::Weapon && !item->weaponSkill.empty())
        {
            state.equippedWeaponSkill = item->weaponSkill;
        }
    }

    state.hp = std::min(state.hp, state.maxHp);
    state.mana = std::min(state.mana, state.maxMana);
}

uint32_t ItemRules::merchantBuyPrice(
    const content::ItemDef& item,
    uint16_t merchantSkill,
    uint16_t charisma,
    uint32_t listedPrice)
{
    const uint32_t basePrice = listedPrice > 0 ? listedPrice : item.vendorValue;
    const float modifier = merchantPriceModifier(merchantSkill, charisma);
    return std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<float>(basePrice) * modifier));
}

uint32_t ItemRules::merchantSellPrice(
    const content::ItemDef& item,
    uint16_t merchantSkill,
    uint16_t charisma)
{
    const float modifier = merchantPriceModifier(merchantSkill, charisma);
    const uint32_t baseSell = std::max<uint32_t>(1, item.vendorValue / 2);
    return std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<float>(baseSell) * (1.0f + (1.0f - modifier))));
}

} // namespace tbeq::items
