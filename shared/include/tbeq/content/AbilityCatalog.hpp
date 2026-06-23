#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::content
{

enum class AbilityTargetType : uint8_t
{
    SingleEnemy = 0,
    Self = 1,
};

enum class AbilityEffectType : uint8_t
{
    Damage = 0,
    DamageStun = 1,
    DamageSnare = 2,
};

struct AbilityDef
{
    std::string id;
    std::string name;
    std::string classId;
    AbilityTargetType targetType = AbilityTargetType::SingleEnemy;
    AbilityEffectType effect = AbilityEffectType::Damage;
    int32_t baseValue = 0;
    std::string linkedSkill;
    std::string skillCheck;
    uint16_t minLevel = 1;
    uint16_t stunTurns = 0;
    uint16_t snareTurns = 0;
};

class AbilityCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);
    const AbilityDef* findAbility(const std::string& abilityId) const;
    std::vector<const AbilityDef*> abilitiesForClass(const std::string& classId, uint16_t level) const;

private:
    std::unordered_map<std::string, AbilityDef> abilities_;
};

} // namespace tbeq::content
