#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbeq::content
{

struct MerchantBuyEntry
{
    std::string itemId;
    uint32_t price = 0;
    uint16_t quantity = 0;
};

struct NpcDef
{
    std::string id;
    std::string name;
    std::vector<std::string> roles;
    std::vector<MerchantBuyEntry> merchantBuyStock;
    std::vector<std::string> merchantSellItemIds;
};

class NpcCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);
    const NpcDef* findNpc(const std::string& npcId) const;
    std::string resolveNpcId(const std::string& npcId) const;

private:
    void registerLegacyAliases();

    std::unordered_map<std::string, NpcDef> npcs_;
    std::unordered_map<std::string, std::string> aliases_;
};

} // namespace tbeq::content
