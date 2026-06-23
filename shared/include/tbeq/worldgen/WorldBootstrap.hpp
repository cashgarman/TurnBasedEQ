#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace tbeq::db
{
class Database;
}

namespace tbeq::worldgen
{

bool ensureWorldInDatabase(
    db::Database& database,
    int64_t seed,
    const std::filesystem::path& dataRoot);

bool ensureZoneGenerated(
    db::Database& database,
    const std::string& zoneId,
    int64_t worldSeed,
    const std::filesystem::path& dataRoot);

} // namespace tbeq::worldgen
