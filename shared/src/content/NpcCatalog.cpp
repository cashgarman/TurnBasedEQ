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
    aliases_.clear();
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
            const std::string canonicalId = def.id;
            npcs_.emplace(canonicalId, std::move(def));
            if (entry.contains("aliases") && entry["aliases"].is_array())
            {
                for (const auto& alias : entry["aliases"])
                {
                    if (alias.is_string())
                    {
                        aliases_.emplace(alias.get<std::string>(), canonicalId);
                    }
                }
            }
        }
    }

    registerLegacyAliases();

    return true;
}

void NpcCatalog::registerLegacyAliases()
{
    const auto addAlias = [this](const std::string& alias, const std::string& canonicalId)
    {
        if (npcs_.find(canonicalId) != npcs_.end())
        {
            aliases_.emplace(alias, canonicalId);
        }
    };

    addAlias("merchant_starter_1", "starter_city_merchant_1");
    addAlias("merchant_starter_2", "starter_city_merchant_2");
}

const NpcDef* NpcCatalog::findNpc(const std::string& npcId) const
{
    const auto direct = npcs_.find(npcId);
    if (direct != npcs_.end())
    {
        return &direct->second;
    }

    const auto alias = aliases_.find(npcId);
    if (alias != aliases_.end())
    {
        const auto resolved = npcs_.find(alias->second);
        if (resolved != npcs_.end())
        {
            return &resolved->second;
        }
    }

    return nullptr;
}

std::string NpcCatalog::resolveNpcId(const std::string& npcId) const
{
    if (npcs_.find(npcId) != npcs_.end())
    {
        return npcId;
    }

    const auto alias = aliases_.find(npcId);
    if (alias != aliases_.end())
    {
        return alias->second;
    }

    return npcId;
}

} // namespace tbeq::content
