#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "tbeq/world/TileDefCatalog.hpp"
#include "tbeq/world/ZoneGrid.hpp"
#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

TEST_CASE("zone grid collision respects tile_defs", "[world][collision]")
{
    tbeq::world::TileDefCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_defs.json"));
    REQUIRE(catalog.isWalkable("grass"));
    REQUIRE_FALSE(catalog.isWalkable("stone_wall"));
    REQUIRE(catalog.isWalkable("shallow_water"));

    tbeq::db::Database database(":memory:");
    REQUIRE(database.open());
    REQUIRE(database.initializeSchema());

    tbeq::worldgen::WorldGenerator generator(42, TBEQ_DATA_ROOT);
    REQUIRE(generator.writeToDatabase(database));

    tbeq::world::ZoneGrid grid;
    REQUIRE(grid.loadFromDatabase(database, "starter_city", catalog));
    REQUIRE(grid.isWalkable(32, 32));
    REQUIRE_FALSE(grid.isWalkable(0, 0));
    REQUIRE(grid.canStepTo(32, 32, 32, 33));
    REQUIRE_FALSE(grid.canStepTo(32, 32, 31, 31));
}

TEST_CASE("portal lookup returns destination", "[world][collision]")
{
    tbeq::world::TileDefCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_defs.json"));

    tbeq::db::Database database(":memory:");
    REQUIRE(database.open());
    REQUIRE(database.initializeSchema());

    tbeq::worldgen::WorldGenerator generator(42, TBEQ_DATA_ROOT);
    REQUIRE(generator.writeToDatabase(database));

    tbeq::world::ZoneGrid grid;
    REQUIRE(grid.loadFromDatabase(database, "starter_city", catalog));

    const auto portal = grid.portalAt(32, 8);
    REQUIRE(portal.has_value());
    REQUIRE(portal->destZoneId == "starter_hunting");
}
