#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::content
{

enum class SpellTargetType : uint8_t
{
    SingleEnemy = 0,
    SingleAlly = 1,
    Self = 2,
};

enum class SpellEffectType : uint8_t
{
    Damage = 0,
    Heal = 1,
    Dot = 2,
};

struct SpellDef
{
    std::string id;
    std::string name;
    std::string classId;
    uint16_t manaCost = 0;
    std::string school;
    SpellTargetType targetType = SpellTargetType::SingleEnemy;
    SpellEffectType effect = SpellEffectType::Damage;
    int32_t baseValue = 0;
    std::string linkedSkill;
    uint16_t minLevel = 1;
    uint16_t dotTurns = 0;
};

class SpellCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);
    const SpellDef* findSpell(const std::string& spellId) const;
    std::vector<const SpellDef*> spellsForClass(const std::string& classId, uint16_t level) const;

private:
    std::unordered_map<std::string, SpellDef> spells_;
};

} // namespace tbeq::content
