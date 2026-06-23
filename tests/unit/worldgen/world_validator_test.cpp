#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "TempDatabase.hpp"
#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"

TEST_CASE("WorldValidator accepts generated seed 42 world", "[worldgen]")
{
#ifdef TBEQ_REPO_ROOT
    const std::filesystem::path dataRoot = std::filesystem::path(TBEQ_REPO_ROOT) / "data";
#else
    const std::filesystem::path dataRoot = "data";
#endif

    tbeq::worldgen::WorldGenerator generator(42, dataRoot);
    const auto report = generator.validateOnly();
    REQUIRE(report.ok);
}

TEST_CASE("tbeq_worldgen writes MVP world to database", "[worldgen]")
{
    tbeq::test::TempDatabase tempDb;
    REQUIRE(tempDb.generateWorld(42));

    tbeq::db::Database database(tempDb.path().string());
    REQUIRE(database.open());
    REQUIRE(database.hasWorld());

    const auto zoneIds = database.listZoneIds();
    REQUIRE(zoneIds.size() == 3);
    REQUIRE(database.loadZoneTiles("starter_city").has_value());
    REQUIRE(database.loadZoneTiles("starter_hunting").has_value());
    REQUIRE(database.loadZoneTiles("starter_dungeon").has_value());
}
