#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::content
{

struct LootEntry
{
    std::string itemId;
    uint16_t minQuantity = 1;
    uint16_t maxQuantity = 1;
    uint16_t dropChance = 100;
};

struct MobDef
{
    std::string id;
    std::string name;
    uint16_t level = 1;
    uint16_t hp = 30;
    uint16_t offense = 10;
    uint16_t defense = 10;
    uint16_t agi = 60;
    bool isNamed = false;
    bool isBoss = false;
    std::string bossScriptId;
    std::vector<LootEntry> loot;
};

class MobCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    const MobDef* findMob(const std::string& mobId) const;
    std::vector<std::string> resolveMobTable(const std::string& tableId) const;
    std::vector<std::string> allMobIds() const;

private:
    std::unordered_map<std::string, MobDef> mobs_;
    std::unordered_map<std::string, std::vector<std::string>> mobTables_;
};

} // namespace tbeq::content
