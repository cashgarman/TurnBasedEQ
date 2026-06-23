#include "tbeq/combat/BossScriptCatalog.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::combat
{

namespace
{

BossTargetMode parseTargetMode(const std::string& value)
{
    if (value == "highest_threat")
    {
        return BossTargetMode::HighestThreat;
    }
    if (value == "random")
    {
        return BossTargetMode::Random;
    }
    return BossTargetMode::LowestHp;
}

} // namespace

bool BossScriptCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_object())
    {
        return false;
    }

    scripts_.clear();
    for (const auto& [scriptId, scriptJson] : json.items())
    {
        BossScriptDef script;
        script.id = scriptId;
        if (!scriptJson.contains("phases") || !scriptJson["phases"].is_array())
        {
            continue;
        }

        for (const auto& phaseJson : scriptJson["phases"])
        {
            BossPhaseDef phase;
            phase.hpPercentMin = phaseJson.value("hpPercentMin", static_cast<uint16_t>(0));
            phase.targetMode = parseTargetMode(phaseJson.value("targetMode", std::string{"lowest_hp"}));
            phase.meleeWeight = phaseJson.value("meleeWeight", static_cast<uint16_t>(80));
            phase.defendWeight = phaseJson.value("defendWeight", static_cast<uint16_t>(20));
            phase.damageMultiplier = phaseJson.value("damageMultiplier", 1.0f);
            phase.announce = phaseJson.value("announce", std::string{});
            script.phases.push_back(std::move(phase));
        }

        if (!script.phases.empty())
        {
            std::sort(script.phases.begin(), script.phases.end(), [](const BossPhaseDef& a, const BossPhaseDef& b)
            {
                return a.hpPercentMin > b.hpPercentMin;
            });
            scripts_[scriptId] = std::move(script);
        }
    }

    return !scripts_.empty();
}

const BossScriptDef* BossScriptCatalog::findScript(const std::string& scriptId) const
{
    const auto it = scripts_.find(scriptId);
    if (it == scripts_.end())
    {
        return nullptr;
    }
    return &it->second;
}

const BossPhaseDef* BossScriptCatalog::activePhase(
    const BossScriptDef& script,
    uint16_t currentHp,
    uint16_t maxHp) const
{
    if (script.phases.empty() || maxHp == 0)
    {
        return nullptr;
    }

    const uint16_t hpPercent = static_cast<uint16_t>((static_cast<uint32_t>(currentHp) * 100u) / maxHp);
    for (const auto& phase : script.phases)
    {
        if (hpPercent >= phase.hpPercentMin)
        {
            return &phase;
        }
    }

    return &script.phases.back();
}

} // namespace tbeq::combat
