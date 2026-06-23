#include "TempDatabase.hpp"

#include <random>
#include <sstream>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"

namespace tbeq::test
{

namespace
{

std::filesystem::path makeTempDbPath()
{
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(100000, 999999);
    std::ostringstream stream;
    stream << "tbeq_test_" << distribution(generator) << ".db";
    return std::filesystem::temp_directory_path() / stream.str();
}

} // namespace

TempDatabase::TempDatabase()
    : path_(makeTempDbPath())
{
    tbeq::db::Database database(path_.string());
    database.open();
    database.initializeSchema();
}

TempDatabase::~TempDatabase()
{
    std::error_code ec;
    std::filesystem::remove(path_, ec);
}

bool TempDatabase::generateWorld(int64_t seed) const
{
    tbeq::db::Database database(path_.string());
    if (!database.open())
    {
        return false;
    }

#ifdef TBEQ_REPO_ROOT
    const std::filesystem::path dataRoot = std::filesystem::path(TBEQ_REPO_ROOT) / "data";
#else
    const std::filesystem::path dataRoot = "data";
#endif

    tbeq::worldgen::WorldGenerator generator(seed, dataRoot);
    return generator.writeToDatabase(database);
}

} // namespace tbeq::test
