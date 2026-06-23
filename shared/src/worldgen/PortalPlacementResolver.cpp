#include "tbeq/worldgen/PortalPlacementResolver.hpp"

#include <string>
#include <unordered_map>
#include <utility>

namespace tbeq::worldgen
{

namespace
{

struct PortalKey
{
    std::string linkId;
    std::string fromZoneId;

    bool operator==(const PortalKey& other) const
    {
        return linkId == other.linkId && fromZoneId == other.fromZoneId;
    }
};

struct PortalKeyHash
{
    std::size_t operator()(const PortalKey& key) const
    {
        return std::hash<std::string>{}(key.linkId + "\0" + key.fromZoneId);
    }
};

ResolvedPortal makePortal(int32_t tileX, int32_t tileY, int32_t destX, int32_t destY)
{
    return ResolvedPortal{tileX, tileY, destX, destY};
}

const std::unordered_map<PortalKey, ResolvedPortal, PortalKeyHash>& mvpPortalTable()
{
    static const std::unordered_map<PortalKey, ResolvedPortal, PortalKeyHash> table = {
        {{"city_to_hunting", "starter_city"}, makePortal(32, 8, 64, 64)},
        {{"hunting_to_city", "starter_hunting"}, makePortal(64, 8, 32, 56)},
        {{"hunting_to_dungeon", "starter_hunting"}, makePortal(120, 64, 4, 24)},
        {{"dungeon_to_hunting", "starter_dungeon"}, makePortal(4, 24, 118, 64)},
    };
    return table;
}

} // namespace

std::optional<ResolvedPortal> PortalPlacementResolver::resolve(
    const ZoneLink& link,
    int64_t worldSeed)
{
    (void)worldSeed;
    const PortalKey key{link.linkId, link.fromZoneId};
    const auto& table = mvpPortalTable();
    const auto it = table.find(key);
    if (it == table.end())
    {
        return std::nullopt;
    }
    return it->second;
}

} // namespace tbeq::worldgen
