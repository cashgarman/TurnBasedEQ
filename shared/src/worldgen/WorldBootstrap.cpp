#include "tbeq/worldgen/WorldBootstrap.hpp"

#include <spdlog/spdlog.h>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"

namespace tbeq::worldgen
{

namespace
{

bool isWorldComplete(const db::Database& database)
{
    if (!database.hasWorld())
    {
        return false;
    }

    if (database.loadZoneNpcSlots("starter_city").empty())
    {
        return false;
    }

    if (database.loadZoneSpawns("starter_hunting").empty())
    {
        return false;
    }

    if (database.loadZoneNpcSlots("starter_hunting").empty())
    {
        return false;
    }

    return true;
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

    if (isWorldComplete(database))
    {
        spdlog::info("World already present in database");
        return true;
    }

    if (database.hasWorld())
    {
        spdlog::warn("World data incomplete; regenerating seed {}", seed);
        if (!database.clearWorld())
        {
            spdlog::error("Failed to clear incomplete world data");
            return false;
        }
    }
    else
    {
        spdlog::info("No world in database; generating seed {}", seed);
    }

    WorldGenerator generator(seed, dataRoot);
    if (!generator.writeToDatabase(database))
    {
        spdlog::error("World generation failed");
        return false;
    }

    spdlog::info("World generation complete");
    return true;
}

} // namespace tbeq::worldgen
