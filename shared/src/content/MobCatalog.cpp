#include "tbeq/content/MobCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

bool MobCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;

    mobs_.clear();
    mobTables_.clear();

    if (json.contains("mobTables") && json["mobTables"].is_object())
    {
        for (const auto& [tableId, tableJson] : json["mobTables"].items())
        {
            std::vector<std::string> mobIds;
            if (tableJson.contains("mobs") && tableJson["mobs"].is_array())
            {
                for (const auto& mobId : tableJson["mobs"])
                {
                    mobIds.push_back(mobId.get<std::string>());
                }
            }
            mobTables_[tableId] = std::move(mobIds);
        }
    }

    if (!json.contains("mobs") || !json["mobs"].is_object())
    {
        return false;
    }

    for (const auto& [mobId, mobJson] : json["mobs"].items())
    {
        MobDef mob;
        mob.id = mobId;
        mob.name = mobJson.value("name", mobId);
        mob.level = mobJson.value("level", static_cast<uint16_t>(1));
        mob.hp = mobJson.value("hp", static_cast<uint16_t>(30));
        mob.offense = mobJson.value("offense", static_cast<uint16_t>(10));
        mob.defense = mobJson.value("defense", static_cast<uint16_t>(10));
        mob.agi = mobJson.value("agi", static_cast<uint16_t>(60));

        if (mobJson.contains("loot") && mobJson["loot"].is_array())
        {
            for (const auto& lootJson : mobJson["loot"])
            {
                LootEntry entry;
                entry.itemId = lootJson.value("itemId", std::string{});
                entry.minQuantity = lootJson.value("minQuantity", static_cast<uint16_t>(1));
                entry.maxQuantity = lootJson.value("maxQuantity", static_cast<uint16_t>(1));
                entry.dropChance = lootJson.value("dropChance", static_cast<uint16_t>(100));
                if (!entry.itemId.empty())
                {
                    mob.loot.push_back(std::move(entry));
                }
            }
        }

        mobs_[mobId] = std::move(mob);
    }

    return !mobs_.empty();
}

const MobDef* MobCatalog::findMob(const std::string& mobId) const
{
    const auto it = mobs_.find(mobId);
    if (it == mobs_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> MobCatalog::resolveMobTable(const std::string& tableId) const
{
    const auto it = mobTables_.find(tableId);
    if (it == mobTables_.end())
    {
        return {};
    }
    return it->second;
}

std::vector<std::string> MobCatalog::allMobIds() const
{
    std::vector<std::string> ids;
    ids.reserve(mobs_.size());
    for (const auto& [mobId, _] : mobs_)
    {
        ids.push_back(mobId);
    }
    return ids;
}

} // namespace tbeq::content
