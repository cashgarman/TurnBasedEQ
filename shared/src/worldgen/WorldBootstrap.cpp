#include "tbeq/worldgen/WorldBootstrap.hpp"

#include <spdlog/spdlog.h>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"
#include "tbeq/worldgen/WorldValidator.hpp"
#include "tbeq/worldgen/ZoneGenerator.hpp"
#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

namespace tbeq::worldgen
{

namespace
{

bool isWorldSkeletonComplete(const db::Database& database)
{
    if (!database.hasWorld())
    {
        return false;
    }

    const auto zoneIds = database.listZoneIds();
    if (zoneIds.size() != 3)
    {
        return false;
    }

    const auto links = database.loadAllZoneLinks();
    if (links.size() != 4)
    {
        return false;
    }

    for (const auto& zoneId : zoneIds)
    {
        const auto metadata = database.loadZoneMetadata(zoneId);
        if (!metadata || metadata->zoneType.empty())
        {
            return false;
        }
    }

    return true;
}

std::vector<ZoneLink> toZoneLinks(const std::vector<db::ZoneLinkRecord>& records)
{
    std::vector<ZoneLink> links;
    links.reserve(records.size());
    for (const auto& record : records)
    {
        links.push_back(
            ZoneLink{
                record.linkId,
                record.fromZoneId,
                record.toZoneId,
                record.fromEdge,
                record.toEdge,
                record.label});
    }
    return links;
}

} // namespace

bool ensureWorldInDatabase(
    db::Database& database,
    int64_t seed,
    const std::filesystem::path& dataRoot)
{
    if (!database.initializeSchema())
    {
        return false;
    }

    if (isWorldSkeletonComplete(database))
    {
        spdlog::info("World skeleton already present in database");
        return true;
    }

    if (database.hasWorld())
    {
        spdlog::warn("World skeleton incomplete; regenerating seed {}", seed);
        if (!database.clearWorld())
        {
            spdlog::error("Failed to clear incomplete world data");
            return false;
        }
    }
    else
    {
        spdlog::info("No world in database; generating skeleton seed {}", seed);
    }

    WorldGenerator generator(seed, dataRoot);
    if (!generator.writeWorldSkeletonToDatabase(database))
    {
        spdlog::error("World skeleton generation failed");
        return false;
    }

    spdlog::info("World skeleton generation complete");
    return true;
}

bool ensureZoneGenerated(
    db::Database& database,
    const std::string& zoneId,
    int64_t worldSeed,
    const std::filesystem::path& dataRoot)
{
    if (database.hasZoneContent(zoneId))
    {
        return true;
    }

    const auto metadata = database.loadZoneMetadata(zoneId);
    if (!metadata)
    {
        spdlog::error("Zone metadata missing for {}", zoneId);
        return false;
    }

    ZoneTypeCatalog catalog;
    if (!catalog.loadFromFile(dataRoot / "worldgen" / "zone_templates.json"))
    {
        spdlog::error("Failed to load zone_templates.json");
        return false;
    }

    const int64_t seed = metadata->worldSeed != 0 ? metadata->worldSeed : worldSeed;
    const auto outgoingRecords = database.loadOutgoingZoneLinks(zoneId);
    const auto outgoingLinks = toZoneLinks(outgoingRecords);

    ZoneGenerator generator(seed, catalog);
    GeneratedZone zone = generator.generate(*metadata, outgoingLinks);
    if (zone.tiles.empty())
    {
        spdlog::error("Zone generation produced no tiles for {}", zoneId);
        return false;
    }

    WorldValidator validator(loadValidatorRules(dataRoot));
    const auto report = validator.validateZone(zone, dataRoot);
    if (!report.ok)
    {
        for (const auto& error : report.errors)
        {
            spdlog::error("Zone validation failed for {}: {}", zoneId, error);
        }
        return false;
    }

    if (!database.beginTransaction())
    {
        spdlog::error("Failed to begin zone persist transaction for {}", zoneId);
        return false;
    }

    if (!persistGeneratedZone(database, zone))
    {
        database.rollbackTransaction();
        spdlog::error("Failed to persist generated zone {}", zoneId);
        return false;
    }

    if (!database.commitTransaction())
    {
        database.rollbackTransaction();
        spdlog::error("Failed to commit zone persist transaction for {}", zoneId);
        return false;
    }

    spdlog::info("Zone generated and persisted: {} (type={})", zoneId, metadata->zoneType);
    return true;
}

} // namespace tbeq::worldgen
