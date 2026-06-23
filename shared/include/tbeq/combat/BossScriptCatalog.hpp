#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::combat
{

enum class BossTargetMode : uint8_t
{
    LowestHp = 0,
    HighestThreat = 1,
    Random = 2,
};

struct BossPhaseDef
{
    uint16_t hpPercentMin = 0;
    BossTargetMode targetMode = BossTargetMode::LowestHp;
    uint16_t meleeWeight = 80;
    uint16_t defendWeight = 20;
    float damageMultiplier = 1.0f;
    std::string announce;
};

struct BossScriptDef
{
    std::string id;
    std::vector<BossPhaseDef> phases;
};

class BossScriptCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    const BossScriptDef* findScript(const std::string& scriptId) const;
    const BossPhaseDef* activePhase(const BossScriptDef& script, uint16_t currentHp, uint16_t maxHp) const;

private:
    std::unordered_map<std::string, BossScriptDef> scripts_;
};

} // namespace tbeq::combat
