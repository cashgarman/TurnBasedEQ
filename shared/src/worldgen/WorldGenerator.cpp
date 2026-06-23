#include "tbeq/worldgen/WorldGenerator.hpp"

#include <chrono>
#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/worldgen/WorldLayoutGraph.hpp"
#include "tbeq/worldgen/WorldValidator.hpp"
#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

namespace tbeq::worldgen
{

namespace
{

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
    return buildWorldSkeleton();
}

GeneratedWorld WorldGenerator::buildWorldSkeleton() const
{
    ZoneTypeCatalog catalog;
    if (!catalog.loadFromFile(dataRoot_ / "worldgen" / "zone_templates.json"))
    {
        return {};
    }

    GeneratedWorld world = buildStarterWorldGraph(catalog);
    world.seed = seed_;
    return world;
}

ValidationReport WorldGenerator::validateSkeletonOnly() const
{
    const auto world = buildWorldSkeleton();
    WorldValidator validator(loadValidatorRules(dataRoot_));
    return validator.validateSkeleton(world);
}

ValidationReport WorldGenerator::validateOnly() const
{
    return validateSkeletonOnly();
}

bool WorldGenerator::writeWorldSkeletonToDatabase(db::Database& database) const
{
    const auto world = buildWorldSkeleton();
    if (world.zones.empty())
    {
        spdlog::error("Failed to build world skeleton (missing zone templates?)");
        return false;
    }

    WorldValidator validator(loadValidatorRules(dataRoot_));
    const auto report = validator.validateSkeleton(world);
    if (!report.ok)
    {
        for (const auto& error : report.errors)
        {
            spdlog::error("World skeleton validation failed: {}", error);
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
                zone.zoneType,
                zone.biome,
                zone.tileStyle,
                zone.width,
                zone.height,
                zone.isSafe))
        {
            return false;
        }
    }

    for (const auto& link : world.links)
    {
        db::ZoneLinkRecord record;
        record.linkId = link.linkId;
        record.fromZoneId = link.fromZoneId;
        record.toZoneId = link.toZoneId;
        record.fromEdge = link.fromEdge;
        record.toEdge = link.toEdge;
        record.label = link.label;
        if (!database.insertZoneLink(record))
        {
            return false;
        }
    }

    return true;
}

bool WorldGenerator::writeToDatabase(db::Database& database) const
{
    return writeWorldSkeletonToDatabase(database);
}

} // namespace tbeq::worldgen
