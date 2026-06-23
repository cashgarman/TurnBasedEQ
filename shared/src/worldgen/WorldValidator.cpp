#include "tbeq/worldgen/WorldValidator.hpp"

#include <fstream>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace tbeq::worldgen
{

namespace
{

bool isWalkableCollision(const std::string& collision)
{
    return collision == "walkable" || collision == "water";
}

std::unordered_map<std::string, std::string> loadTileCollisions(const std::filesystem::path& dataRoot)
{
    std::unordered_map<std::string, std::string> collisions;
    const auto tileDefsPath = dataRoot / "tile_defs.json";
    std::ifstream input(tileDefsPath);
    if (!input.is_open())
    {
        return collisions;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_array())
    {
        return collisions;
    }

    for (const auto& entry : json)
    {
        if (!entry.contains("id") || !entry.contains("collision"))
        {
            continue;
        }
        collisions[entry["id"].get<std::string>()] = entry["collision"].get<std::string>();
    }
    return collisions;
}

bool isWalkableTile(const std::unordered_map<std::string, std::string>& collisions, const std::string& tileId)
{
    const auto it = collisions.find(tileId);
    if (it == collisions.end())
    {
        return false;
    }
    return isWalkableCollision(it->second);
}

int32_t countSingleTileChokes(const GeneratedZone& zone, const std::unordered_map<std::string, std::string>& collisions)
{
    int32_t chokeCount = 0;
    for (int32_t y = 1; y < zone.height - 1; ++y)
    {
        for (int32_t x = 1; x < zone.width - 1; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y * zone.width + x);
            const std::string& tileId = zone.tiles[index];
            if (!isWalkableTile(collisions, tileId))
            {
                continue;
            }

            int walkableNeighbors = 0;
            const int offsets[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            for (const auto& offset : offsets)
            {
                const std::size_t neighborIndex = static_cast<std::size_t>((y + offset[1]) * zone.width + (x + offset[0]));
                if (isWalkableTile(collisions, zone.tiles[neighborIndex]))
                {
                    ++walkableNeighbors;
                }
            }

            if (walkableNeighbors <= 1)
            {
                ++chokeCount;
            }
        }
    }
    return chokeCount;
}

double walkableRatio(const GeneratedZone& zone, const std::unordered_map<std::string, std::string>& collisions)
{
    if (zone.tiles.empty())
    {
        return 0.0;
    }

    std::size_t walkableCount = 0;
    for (const auto& tileId : zone.tiles)
    {
        if (isWalkableTile(collisions, tileId))
        {
            ++walkableCount;
        }
    }
    return static_cast<double>(walkableCount) / static_cast<double>(zone.tiles.size());
}

std::unordered_map<std::string, std::vector<std::string>> buildZoneGraph(const GeneratedWorld& world)
{
    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const auto& zone : world.zones)
    {
        graph.emplace(zone.id, std::vector<std::string>{});
        for (const auto& portal : zone.portals)
        {
            graph[zone.id].push_back(portal.destZoneId);
        }
    }
    return graph;
}

bool graphIsConnected(const GeneratedWorld& world)
{
    if (world.zones.empty())
    {
        return false;
    }

    const auto graph = buildZoneGraph(world);
    const std::string startZoneId = world.zones.front().id;
    std::unordered_set<std::string> visited;
    std::queue<std::string> pending;
    pending.push(startZoneId);
    visited.insert(startZoneId);

    while (!pending.empty())
    {
        const std::string current = pending.front();
        pending.pop();
        const auto it = graph.find(current);
        if (it == graph.end())
        {
            continue;
        }

        for (const auto& neighbor : it->second)
        {
            if (visited.insert(neighbor).second)
            {
                pending.push(neighbor);
            }
        }
    }

    for (const auto& zone : world.zones)
    {
        if (visited.find(zone.id) == visited.end())
        {
            return false;
        }
    }
    return true;
}

bool portalDestinationsExist(const GeneratedWorld& world)
{
    std::unordered_set<std::string> zoneIds;
    for (const auto& zone : world.zones)
    {
        zoneIds.insert(zone.id);
    }

    for (const auto& zone : world.zones)
    {
        for (const auto& portal : zone.portals)
        {
            if (zoneIds.find(portal.destZoneId) == zoneIds.end())
            {
                return false;
            }

            const std::size_t index = static_cast<std::size_t>(portal.tileY * zone.width + portal.tileX);
            if (index >= zone.tiles.size() || zone.tiles[index] != "portal_pad")
            {
                return false;
            }
        }
    }
    return true;
}

bool graphIsConnectedFromLinks(const GeneratedWorld& world)
{
    if (world.zones.empty())
    {
        return false;
    }

    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const auto& zone : world.zones)
    {
        graph.emplace(zone.id, std::vector<std::string>{});
    }

    for (const auto& link : world.links)
    {
        graph[link.fromZoneId].push_back(link.toZoneId);
    }

    const std::string startZoneId = world.zones.front().id;
    std::unordered_set<std::string> visited;
    std::queue<std::string> pending;
    pending.push(startZoneId);
    visited.insert(startZoneId);

    while (!pending.empty())
    {
        const std::string current = pending.front();
        pending.pop();
        const auto it = graph.find(current);
        if (it == graph.end())
        {
            continue;
        }

        for (const auto& neighbor : it->second)
        {
            if (visited.insert(neighbor).second)
            {
                pending.push(neighbor);
            }
        }
    }

    for (const auto& zone : world.zones)
    {
        if (visited.find(zone.id) == visited.end())
        {
            return false;
        }
    }
    return true;
}

bool zoneTypesValid(const GeneratedWorld& world)
{
    for (const auto& zone : world.zones)
    {
        if (zone.zoneType.empty())
        {
            return false;
        }
    }
    return true;
}

bool linkEndpointsExist(const GeneratedWorld& world)
{
    std::unordered_set<std::string> zoneIds;
    for (const auto& zone : world.zones)
    {
        zoneIds.insert(zone.id);
    }

    for (const auto& link : world.links)
    {
        if (zoneIds.find(link.fromZoneId) == zoneIds.end()
            || zoneIds.find(link.toZoneId) == zoneIds.end())
        {
            return false;
        }
    }
    return true;
}

} // namespace

WorldValidator::WorldValidator(ValidatorRules rules)
    : rules_(std::move(rules))
{
}

ValidationReport WorldValidator::validate(const GeneratedWorld& world, const std::filesystem::path& dataRoot) const
{
    ValidationReport report;
    report.ok = true;

    if (world.zones.empty())
    {
        report.ok = false;
        report.errors.push_back("World has no zones");
        return report;
    }

    const auto collisions = loadTileCollisions(dataRoot);
    if (collisions.empty())
    {
        report.ok = false;
        report.errors.push_back("Unable to load tile collision data");
        return report;
    }

    if (rules_.requireConnectedGraph && !graphIsConnected(world))
    {
        report.ok = false;
        report.errors.push_back("Zone graph is not fully connected");
    }

    if (!portalDestinationsExist(world))
    {
        report.ok = false;
        report.errors.push_back("One or more portals reference missing zones or invalid portal tiles");
    }

    for (const auto& zone : world.zones)
    {
        if (static_cast<int32_t>(zone.tiles.size()) != zone.width * zone.height)
        {
            report.ok = false;
            report.errors.push_back("Zone " + zone.id + " tile grid size mismatch");
            continue;
        }

        const double ratio = walkableRatio(zone, collisions);
        if (ratio < rules_.minWalkableRatio)
        {
            report.ok = false;
            report.errors.push_back("Zone " + zone.id + " walkable ratio below minimum");
        }

        const int32_t chokes = countSingleTileChokes(zone, collisions);
        if (chokes > rules_.maxSingleTileChokeCount)
        {
            report.ok = false;
            report.errors.push_back("Zone " + zone.id + " has too many single-tile chokes");
        }
    }

    return report;
}

ValidationReport WorldValidator::validateZone(const GeneratedZone& zone, const std::filesystem::path& dataRoot) const
{
    ValidationReport report;
    report.ok = true;

    const auto collisions = loadTileCollisions(dataRoot);
    if (collisions.empty())
    {
        report.ok = false;
        report.errors.push_back("Unable to load tile collision data");
        return report;
    }

    if (static_cast<int32_t>(zone.tiles.size()) != zone.width * zone.height)
    {
        report.ok = false;
        report.errors.push_back("Zone " + zone.id + " tile grid size mismatch");
        return report;
    }

    const double ratio = walkableRatio(zone, collisions);
    if (ratio < rules_.minWalkableRatio)
    {
        report.ok = false;
        report.errors.push_back("Zone " + zone.id + " walkable ratio below minimum");
    }

    const int32_t chokes = countSingleTileChokes(zone, collisions);
    if (chokes > rules_.maxSingleTileChokeCount)
    {
        report.ok = false;
        report.errors.push_back("Zone " + zone.id + " has too many single-tile chokes");
    }

    for (const auto& portal : zone.portals)
    {
        const std::size_t index = static_cast<std::size_t>(portal.tileY * zone.width + portal.tileX);
        if (index >= zone.tiles.size() || zone.tiles[index] != "portal_pad")
        {
            report.ok = false;
            report.errors.push_back("Zone " + zone.id + " portal tile invalid at ("
                + std::to_string(portal.tileX) + "," + std::to_string(portal.tileY) + ")");
        }
    }

    return report;
}

ValidationReport WorldValidator::validateSkeleton(const GeneratedWorld& world) const
{
    ValidationReport report;
    report.ok = true;

    if (world.zones.empty())
    {
        report.ok = false;
        report.errors.push_back("World has no zones");
        return report;
    }

    if (!zoneTypesValid(world))
    {
        report.ok = false;
        report.errors.push_back("One or more zones missing zone_type");
    }

    if (!linkEndpointsExist(world))
    {
        report.ok = false;
        report.errors.push_back("One or more zone links reference missing zones");
    }

    if (rules_.requireConnectedGraph && !graphIsConnectedFromLinks(world))
    {
        report.ok = false;
        report.errors.push_back("Zone link graph is not fully connected");
    }

    return report;
}

} // namespace tbeq::worldgen
