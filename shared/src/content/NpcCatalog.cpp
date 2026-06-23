#include "tbeq/content/NpcCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::content
{

bool NpcCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input)
    {
        return false;
    }

    nlohmann::json root;
    input >> root;
    if (!root.is_array())
    {
        return false;
    }

    npcs_.clear();
    for (const auto& entry : root)
    {
        NpcDef def;
        def.id = entry.value("id", std::string{});
        def.name = entry.value("name", def.id);

        if (entry.contains("roles") && entry["roles"].is_array())
        {
            for (const auto& role : entry["roles"])
            {
                def.roles.push_back(role.get<std::string>());
            }
        }

        if (entry.contains("merchantStock") && entry["merchantStock"].is_object())
        {
            const auto& stock = entry["merchantStock"];
            if (stock.contains("buy") && stock["buy"].is_array())
            {
                for (const auto& buyEntry : stock["buy"])
                {
                    MerchantBuyEntry buy;
                    buy.itemId = buyEntry.value("itemId", std::string{});
                    buy.price = buyEntry.value("price", static_cast<uint32_t>(0));
                    buy.quantity = buyEntry.value("quantity", static_cast<uint16_t>(0));
                    if (!buy.itemId.empty())
                    {
                        def.merchantBuyStock.push_back(std::move(buy));
                    }
                }
            }
            if (stock.contains("sellItemIds") && stock["sellItemIds"].is_array())
            {
                for (const auto& itemId : stock["sellItemIds"])
                {
                    def.merchantSellItemIds.push_back(itemId.get<std::string>());
                }
            }
        }

        if (!def.id.empty())
        {
            npcs_.emplace(def.id, std::move(def));
        }
    }

    return true;
}

const NpcDef* NpcCatalog::findNpc(const std::string& npcId) const
{
    const auto it = npcs_.find(npcId);
    if (it == npcs_.end())
    {
        return nullptr;
    }
    return &it->second;
}

} // namespace tbeq::content
