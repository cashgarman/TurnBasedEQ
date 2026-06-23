#pragma once

#include <cstdint>
#include <filesystem>

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

} // namespace tbeq::worldgen
