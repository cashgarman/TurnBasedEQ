#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
    std::string classId;
    uint16_t level = 1;
    std::unordered_map<std::string, SkillProgress> skills;
    std::vector<std::string> unlockedSpells;
    std::vector<std::string> unlockedAbilities;
    std::vector<InventoryItem> inventory;
    std::string equippedWeaponSkill = "1h_slash";
    bool godMode = false;

    static CharacterState createDefault(const std::string& classId, uint16_t level);
    static CharacterState fromJson(const std::string& json);
    std::string toJson() const;

    SkillProgress& skillOrDefault(const std::string& skillId);
    uint16_t skillLevel(const std::string& skillId) const;
    void addItem(const std::string& itemId, uint16_t quantity);
    bool knowsSpell(const std::string& spellId) const;
    bool knowsAbility(const std::string& abilityId) const;
    void unlockAllClassContent(
        const std::vector<std::string>& spellIds,
        const std::vector<std::string>& abilityIds);
};

} // namespace tbeq
