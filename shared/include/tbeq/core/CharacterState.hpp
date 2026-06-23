#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/skills/SkillResolver.hpp"

namespace tbeq
{

struct InventoryItem
{
    std::string itemId;
    uint16_t quantity = 1;
};

struct CharacterState
{
    uint16_t hp = 100;
    uint16_t maxHp = 100;
    uint16_t mana = 50;
    uint16_t maxMana = 50;
    uint16_t agi = 75;
    uint16_t cha = 100;
    uint32_t gold = 200;
    std::string classId;
    uint16_t level = 1;
    uint32_t experience = 0;
    std::unordered_map<std::string, SkillProgress> skills;
    std::vector<std::string> unlockedSpells;
    std::vector<std::string> unlockedAbilities;
    std::vector<InventoryItem> inventory;
    std::unordered_map<std::string, std::string> equipment;
    std::string equippedWeaponSkill = "1h_slash";
    bool godMode = false;

    static CharacterState createDefault(const std::string& classId, uint16_t level);
    static CharacterState fromJson(const std::string& json);
    std::string toJson() const;

    SkillProgress& skillOrDefault(const std::string& skillId);
    uint16_t skillLevel(const std::string& skillId) const;
    void addItem(const std::string& itemId, uint16_t quantity);
    bool removeItem(const std::string& itemId, uint16_t quantity);
    uint16_t itemQuantity(const std::string& itemId) const;
    bool knowsSpell(const std::string& spellId) const;
    bool knowsAbility(const std::string& abilityId) const;
    void unlockAllClassContent(
        const std::vector<std::string>& spellIds,
        const std::vector<std::string>& abilityIds);
    std::string equippedItemInSlot(const std::string& slot) const;
};

} // namespace tbeq
