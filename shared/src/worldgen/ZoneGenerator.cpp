#include "tbeq/worldgen/ZoneGenerator.hpp"

#include "tbeq/worldgen/PortalPlacementResolver.hpp"
#include "tbeq/worldgen/ZoneLayoutUtils.hpp"
#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

namespace tbeq::worldgen
{

ZoneGenerator::ZoneGenerator(int64_t worldSeed, const ZoneTypeCatalog& catalog)
    : worldSeed_(worldSeed)
    , catalog_(catalog)
{
}

GeneratedZone ZoneGenerator::generate(
    const db::ZoneMetadataRecord& metadata,
    const std::vector<ZoneLink>& outgoingLinks) const
{
    if (metadata.zoneType == "city")
    {
        return generateCity(metadata, outgoingLinks);
    }
    if (metadata.zoneType == "hunting")
    {
        return generateHunting(metadata, outgoingLinks);
    }
    if (metadata.zoneType == "dungeon")
    {
        return generateDungeon(metadata, outgoingLinks);
    }

    GeneratedZone zone;
    zone.id = metadata.id;
    zone.name = metadata.name;
    zone.role = metadata.role;
    zone.zoneType = metadata.zoneType;
    zone.biome = metadata.biome;
    zone.tileStyle = metadata.tileStyle;
    zone.width = metadata.width;
    zone.height = metadata.height;
    zone.isSafe = metadata.isSafe;
    return zone;
}

void ZoneGenerator::applyPortals(
    GeneratedZone& zone,
    const std::vector<ZoneLink>& outgoingLinks) const
{
    for (const auto& link : outgoingLinks)
    {
        const auto placement = PortalPlacementResolver::resolve(link, worldSeed_);
        if (!placement)
        {
            continue;
        }

        setTile(zone.tiles, zone.width, placement->tileX, placement->tileY, "portal_pad");
        zone.portals.push_back(
            GeneratedZone::Portal{
                placement->tileX,
                placement->tileY,
                link.toZoneId,
                placement->destX,
                placement->destY,
                link.label});
    }
}

GeneratedZone ZoneGenerator::generateCity(
    const db::ZoneMetadataRecord& metadata,
    const std::vector<ZoneLink>& outgoingLinks) const
{
    const auto cityTemplate = catalog_.cityTemplate();
    if (!cityTemplate)
    {
        return {};
    }

    GeneratedZone zone;
    zone.id = metadata.id;
    zone.name = metadata.name;
    zone.role = metadata.role;
    zone.zoneType = metadata.zoneType;
    zone.biome = metadata.biome;
    zone.tileStyle = metadata.tileStyle;
    zone.width = metadata.width;
    zone.height = metadata.height;
    zone.isSafe = metadata.isSafe;
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

    applyPortals(zone, outgoingLinks);

    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"merchant", 24, 28, "starter_city_merchant_1"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"merchant", 38, 28, "starter_city_merchant_2"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 30, 36, "lorekeeper_starter_1"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 34, 36, "lorekeeper_starter_2"});
    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 32, 40, "lorekeeper_starter_3"});

    (void)cityTemplate;
    return zone;
}

GeneratedZone ZoneGenerator::generateHunting(
    const db::ZoneMetadataRecord& metadata,
    const std::vector<ZoneLink>& outgoingLinks) const
{
    const auto huntingTemplate = catalog_.huntingTemplate();
    if (!huntingTemplate)
    {
        return {};
    }

    GeneratedZone zone;
    zone.id = metadata.id;
    zone.name = metadata.name;
    zone.role = metadata.role;
    zone.zoneType = metadata.zoneType;
    zone.biome = metadata.biome;
    zone.tileStyle = metadata.tileStyle;
    zone.width = metadata.width;
    zone.height = metadata.height;
    zone.isSafe = metadata.isSafe;
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

    applyPortals(zone, outgoingLinks);

    zone.spawns.push_back(GeneratedZone::Spawn{"camp_alpha", 40, 88, "forest_tier1", 90});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_beta", 88, 96, "forest_tier1", 90});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_gamma", 96, 40, "forest_tier2", 120});
    zone.spawns.push_back(GeneratedZone::Spawn{"camp_delta", 48, 104, "forest_tier2", 120});
    zone.spawns.push_back(GeneratedZone::Spawn{"trail_scouts", 78, 56, "forest_tier1", 90});

    zone.npcSlots.push_back(GeneratedZone::NpcSlot{"lore", 66, 64, "starter_hunting_guide"});

    (void)huntingTemplate;
    return zone;
}

GeneratedZone ZoneGenerator::generateDungeon(
    const db::ZoneMetadataRecord& metadata,
    const std::vector<ZoneLink>& outgoingLinks) const
{
    const auto dungeonTemplate = catalog_.dungeonTemplate();
    if (!dungeonTemplate)
    {
        return {};
    }

    GeneratedZone zone;
    zone.id = metadata.id;
    zone.name = metadata.name;
    zone.role = metadata.role;
    zone.zoneType = metadata.zoneType;
    zone.biome = metadata.biome;
    zone.tileStyle = metadata.tileStyle;
    zone.width = metadata.width;
    zone.height = metadata.height;
    zone.isSafe = metadata.isSafe;
    zone.tiles = makeFilledGrid(zone.width, zone.height, "stone_wall");

    carveRect(zone.tiles, zone.width, 2, 18, 44, 30, "stone_floor");
    carveRect(zone.tiles, zone.width, 8, 8, 24, 18, "stone_floor");
    carveRect(zone.tiles, zone.width, 22, 6, 42, 24, "stone_floor");
    carveRect(zone.tiles, zone.width, 28, 24, 44, 40, "stone_floor");
    carveRect(zone.tiles, zone.width, 12, 30, 26, 40, "stone_floor");

    carveHorizontalPath(zone.tiles, zone.width, 24, 2, 8, "stone_floor");

    applyPortals(zone, outgoingLinks);

    zone.spawns.push_back(GeneratedZone::Spawn{"goblin_patrol", 16, 14, "goblin_tier1", 75});
    zone.spawns.push_back(GeneratedZone::Spawn{"goblin_boss", 36, 32, "goblin_boss", 600});

    (void)dungeonTemplate;
    return zone;
}

bool persistGeneratedZone(db::Database& database, const GeneratedZone& zone)
{
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

    return true;
}

} // namespace tbeq::worldgen
