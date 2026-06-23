#include "tbeq/worldgen/WorldGenerator.hpp"

#include <chrono>
#include <fstream>
#include <random>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/worldgen/WorldValidator.hpp"

namespace tbeq::worldgen
{

namespace
{

std::vector<std::string> makeFilledGrid(int32_t width, int32_t height, const std::string& tileId)
{
    return std::vector<std::string>(static_cast<std::size_t>(width * height), tileId);
}

void setTile(std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y, const std::string& tileId)
{
    if (x < 0 || y < 0)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(y * width + x);
    if (index < tiles.size())
    {
        tiles[index] = tileId;
    }
}

std::string tileAt(const std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y)
{
    const std::size_t index = static_cast<std::size_t>(y * width + x);
    if (index >= tiles.size())
    {
        return "stone_wall";
    }
    return tiles[index];
}

void carveRect(std::vector<std::string>& tiles, int32_t width, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const std::string& tileId)
{
    for (int32_t y = y0; y <= y1; ++y)
    {
        for (int32_t x = x0; x <= x1; ++x)
        {
            setTile(tiles, width, x, y, tileId);
        }
    }
}

void carveHorizontalPath(std::vector<std::string>& tiles, int32_t width, int32_t y, int32_t x0, int32_t x1, const std::string& tileId)
{
    const int32_t start = std::min(x0, x1);
    const int32_t end = std::max(x0, x1);
    for (int32_t x = start; x <= end; ++x)
    {
        setTile(tiles, width, x, y, tileId);
    }
}

void carveVerticalPath(std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y0, int32_t y1, const std::string& tileId)
{
    const int32_t start = std::min(y0, y1);
    const int32_t end = std::max(y0, y1);
    for (int32_t y = start; y <= end; ++y)
    {
        setTile(tiles, width, x, y, tileId);
    }
}

std::string tilesToJson(const std::vector<std::string>& tiles)
{
    return nlohmann::json(tiles).dump();
}

int64_t nowUnixSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

WorldGenerator::WorldGenerator(int64_t seed, const std::filesystem::path& dataRoot)
    : seed_(seed)
    , dataRoot_(dataRoot)
{
}

ValidatorRules loadValidatorRules(const std::filesystem::path& dataRoot)
{
    ValidatorRules rules;
    const auto rulesPath = dataRoot / "worldgen" / "world_rules.json";
    std::ifstream input(rulesPath);
    if (!input.is_open())
    {
        return rules;
    }

    nlohmann::json json;
    input >> json;
    if (json.contains("validatorRules"))
    {
        const auto& validator = json["validatorRules"];
        if (validator.contains("requireConnectedGraph"))
        {
            rules.requireConnectedGraph = validator["requireConnectedGraph"].get<bool>();
        }
        if (validator.contains("minWalkableRatio"))
        {
            rules.minWalkableRatio = validator["minWalkableRatio"].get<double>();
        }
        if (validator.contains("maxSingleTileChokeCount"))
        {
            rules.maxSingleTileChokeCount = validator["maxSingleTileChokeCount"].get<int32_t>();
        }
    }
    return rules;
}

GeneratedWorld WorldGenerator::generate() const
{
    return buildWorld();
}

ValidationReport WorldGenerator::validateOnly() const
{
    const auto world = buildWorld();
    WorldValidator validator(loadValidatorRules(dataRoot_));
    return validator.validate(world, dataRoot_);
}

GeneratedWorld WorldGenerator::buildWorld() const
{
    GeneratedWorld world;
    world.seed = seed_;
    world.version = "0.1.0";
    world.zones.push_back(buildCityZone());
    world.zones.push_back(buildHuntingZone());
    world.zones.push_back(buildDungeonZone());

    GeneratedWorld mutableWorld = world;
    bindNpcContent(mutableWorld);
    return mutableWorld;
}

GeneratedZone WorldGenerator::buildCityZone() const
{
    GeneratedZone zone;
    zone.id = "starter_city";
    zone.name = "Starter City";
    zone.role = "starter_city";
    zone.biome = "temperate";
    zone.tileStyle = "starter_city";
    zone.width = 64;
    zone.height = 64;
    zone.isSafe = true;
    zone.tiles = makeFilledGrid(zone.width, zone.height, "grass");

    for (int32_t x = 0; x < zone.width; ++x)
    {
        setTile(zone.tiles, zone.width, x, 0, "stone_wall");
        setTile(zone.tiles, zone.width, x, zone.height - 1, "stone_wall");
    }
    for (int32_t y = 0; y < zone.height; ++y)
    {
        setTile(zone.tiles, zone.width, 0, y, "stone_wall");
        setTile(zone.tiles, zone.width, zone.width - 1, y, "stone_wall");
    }

    carveRect(zone.tiles, zone.width, 20, 20, 43, 43, "cobble");
    carveHorizontalPath(zone.tiles, zone.width, 32, 8, 56, "cobble");
    carveVerticalPath(zone.tiles, zone.width, 32, 8, 56, "cobble");

    const int32_t northGateX = 32;
    const int32_t northGateY = 8;
    setTile(zone.tiles, zone.width, northGateX, northGateY, "portal_pad");

    zone.portals.push_back(
        GeneratedZone::Portal{
            northGateX,
            northGateY,
            "starter_hunting",
            64,
            64,
            "To Hunting Grounds"});

    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"merchant", 24, 28, "starter_city_merchant_1"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"merchant", 38, 28, "starter_city_merchant_2"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 30, 36, "lorekeeper_starter_1"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 34, 36, "lorekeeper_starter_2"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 32, 40, "lorekeeper_starter_3"});

    return zone;
}

GeneratedZone WorldGenerator::buildHuntingZone() const
{
    GeneratedZone zone;
    zone.id = "starter_hunting";
    zone.name = "Hunting Grounds";
    zone.role = "hunting";
    zone.biome = "forest";
    zone.tileStyle = "hunting_grounds";
    zone.width = 128;
    zone.height = 128;
    zone.isSafe = false;
    zone.tiles = makeFilledGrid(zone.width, zone.height, "grass");

    for (int32_t x = 0; x < zone.width; ++x)
    {
        setTile(zone.tiles, zone.width, x, 0, "shallow_water");
        setTile(zone.tiles, zone.width, x, zone.height - 1, "shallow_water");
    }
    for (int32_t y = 0; y < zone.height; ++y)
    {
        setTile(zone.tiles, zone.width, 0, y, "shallow_water");
        setTile(zone.tiles, zone.width, zone.width - 1, y, "shallow_water");
    }

    carveVerticalPath(zone.tiles, zone.width, 64, 8, 120, "dirt");
    carveHorizontalPath(zone.tiles, zone.width, 64, 32, 96, "dirt");
    carveHorizontalPath(zone.tiles, zone.width, 96, 40, 88, "dirt");

    const int32_t southPortalX = 64;
    const int32_t southPortalY = 8;
    setTile(zone.tiles, zone.width, southPortalX, southPortalY, "portal_pad");
    zone.portals.push_back(
        GeneratedZone::Portal{
            southPortalX,
            southPortalY,
            "starter_city",
            32,
            56,
            "To Starter City"});

    const int32_t eastPortalX = 120;
    const int32_t eastPortalY = 64;
    setTile(zone.tiles, zone.width, eastPortalX, eastPortalY, "portal_pad");
    zone.portals.push_back(
        GeneratedZone::Portal{
            eastPortalX,
            eastPortalY,
            "starter_dungeon",
            4,
            24,
            "To Goblin Cave"});

    zone.spawns.push_back(GeneratedZone::Spawn{"camp_alpha", 40, 88, "forest_tier1", 90});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_beta", 88, 96, "forest_tier1", 90});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_gamma", 96, 40, "forest_tier2", 120});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_delta", 48, 104, "forest_tier2", 120});
    zone.spawns.push_back(GeneratedZone::Spawn{"trail_scouts", 78, 56, "forest_tier1", 90});

    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 66, 64, "starter_hunting_guide"});

    return zone;
}

GeneratedZone WorldGenerator::buildDungeonZone() const
{
    GeneratedZone zone;
    zone.id = "starter_dungeon";
    zone.name = "Goblin Cave";
    zone.role = "dungeon";
    zone.biome = "cavern";
    zone.tileStyle = "hunting_grounds";
    zone.width = 48;
    zone.height = 48;
    zone.isSafe = false;
    zone.tiles = makeFilledGrid(zone.width, zone.height, "stone_wall");

    carveRect(zone.tiles, zone.width, 2, 18, 44, 30, "stone_floor");
    carveRect(zone.tiles, zone.width, 8, 8, 24, 18, "stone_floor");
    carveRect(zone.tiles, zone.width, 22, 6, 42, 24, "stone_floor");
    carveRect(zone.tiles, zone.width, 28, 24, 44, 40, "stone_floor");
    carveRect(zone.tiles, zone.width, 12, 30, 26, 40, "stone_floor");

    carveHorizontalPath(zone.tiles, zone.width, 24, 2, 8, "stone_floor");
    setTile(zone.tiles, zone.width, 4, 24, "portal_pad");
    zone.portals.push_back(
        GeneratedZone::Portal{
            4,
            24,
            "starter_hunting",
            118,
            64,
            "Exit to Hunting Grounds"});

    zone.spawns.push_back(GeneratedZone::Spawn{"goblin_patrol", 16, 14, "goblin_tier1", 75});
    zone.spawns.push_back(GeneratedZone::Spawn{"goblin_boss", 36, 32, "goblin_boss", 600});

    return zone;
}

void WorldGenerator::bindNpcContent(GeneratedWorld& world) const
{
    (void)dataRoot_;
    for (auto& zone : world.zones)
    {
        int merchantIndex = 1;
        int loreIndex = 1;
        for (auto& slot : zone.npcSlots)
        {
            if (!slot.npcId.empty())
            {
                continue;
            }

            if (slot.slotType == "merchant")
            {
                slot.npcId = zone.id + "_merchant_" + std::to_string(merchantIndex++);
            }
            else if (slot.slotType == "lore")
            {
                slot.npcId = zone.id + "_lore_" + std::to_string(loreIndex++);
            }
            else if (slot.slotType == "quest")
            {
                slot.npcId = zone.id + "_quest_1";
            }
        }
    }
}

bool WorldGenerator::writeToDatabase(db::Database& database) const
{
    const auto world = buildWorld();
    WorldValidator validator(loadValidatorRules(dataRoot_));
    const auto report = validator.validate(world, dataRoot_);
    if (!report.ok)
    {
        for (const auto& error : report.errors)
        {
            spdlog::error("World validation failed: {}", error);
        }
        return false;
    }

    int64_t worldId = 0;
    if (!database.insertWorld(world.seed, world.version, nowUnixSeconds(), worldId))
    {
        return false;
    }

    for (const auto& zone : world.zones)
    {
        if (!database.insertZone(
                zone.id,
                worldId,
                zone.name,
                zone.role,
                zone.biome,
                zone.tileStyle,
                zone.width,
                zone.height,
                zone.isSafe))
        {
            return false;
        }

        if (!database.insertZoneTiles(zone.id, tilesToJson(zone.tiles)))
        {
            return false;
        }

        for (const auto& portal : zone.portals)
        {
            db::ZonePortalRecord record;
            record.zoneId = zone.id;
            record.tileX = portal.tileX;
            record.tileY = portal.tileY;
            record.destZoneId = portal.destZoneId;
            record.destX = portal.destX;
            record.destY = portal.destY;
            record.label = portal.label;
            if (!database.insertZonePortal(record))
            {
                return false;
            }
        }

        for (const auto& spawn : zone.spawns)
        {
            if (!database.insertZoneSpawn(
                    zone.id,
                    spawn.spawnId,
                    spawn.tileX,
                    spawn.tileY,
                    spawn.mobTable,
                    spawn.respawnSeconds))
            {
                return false;
            }
        }

        for (const auto& slot : zone.npcSlots)
        {
            if (!database.insertZoneNpcSlot(zone.id, slot.slotType, slot.tileX, slot.tileY, slot.npcId))
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace tbeq::worldgen
