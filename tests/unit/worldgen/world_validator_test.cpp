#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "TempDatabase.hpp"
#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/PortalPlacementResolver.hpp"
#include "tbeq/worldgen/WorldBootstrap.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"
#include "tbeq/worldgen/WorldLayoutGraph.hpp"
#include "tbeq/worldgen/ZoneGenerator.hpp"
#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT (std::filesystem::path(TBEQ_REPO_ROOT) / "data")
#else
#define TBEQ_DATA_ROOT std::filesystem::path("data")
#endif

namespace
{

std::string tileHash(const tbeq::worldgen::GeneratedZone& zone)
{
    std::size_t hash = 0;
    for (const auto& tile : zone.tiles)
    {
        hash = hash * 1315423911u + std::hash<std::string>{}(tile);
    }
    return std::to_string(hash);
}

tbeq::worldgen::GeneratedZone generateZone(
    tbeq::db::Database& database,
    const std::string& zoneId,
    int64_t seed,
    const std::filesystem::path& dataRoot)
{
    const auto metadata = database.loadZoneMetadata(zoneId);
    REQUIRE(metadata.has_value());

    tbeq::worldgen::ZoneTypeCatalog catalog;
    REQUIRE(catalog.loadFromFile(dataRoot / "worldgen" / "zone_templates.json"));

    const auto outgoingRecords = database.loadOutgoingZoneLinks(zoneId);
    std::vector<tbeq::worldgen::ZoneLink> outgoingLinks;
    for (const auto& record : outgoingRecords)
    {
        outgoingLinks.push_back(
            tbeq::worldgen::ZoneLink{
                record.linkId,
                record.fromZoneId,
                record.toZoneId,
                record.fromEdge,
                record.toEdge,
                record.label});
    }

    tbeq::worldgen::ZoneGenerator generator(seed, catalog);
    return generator.generate(*metadata, outgoingLinks);
}

} // namespace

TEST_CASE("WorldValidator accepts generated seed 42 skeleton", "[worldgen]")
{
    tbeq::worldgen::WorldGenerator generator(42, TBEQ_DATA_ROOT);
    const auto report = generator.validateSkeletonOnly();
    REQUIRE(report.ok);
}

TEST_CASE("world skeleton writes zones and links without tiles", "[worldgen]")
{
    tbeq::test::TempDatabase tempDb;
    REQUIRE(tempDb.generateWorld(42));

    tbeq::db::Database database(tempDb.path().string());
    REQUIRE(database.open());
    REQUIRE(database.hasWorld());

    const auto zoneIds = database.listZoneIds();
    REQUIRE(zoneIds.size() == 3);
    REQUIRE_FALSE(database.hasZoneContent("starter_city"));
    REQUIRE_FALSE(database.hasZoneContent("starter_hunting"));
    REQUIRE_FALSE(database.hasZoneContent("starter_dungeon"));
    REQUIRE(database.loadAllZoneLinks().size() == 4);

    const auto cityMeta = database.loadZoneMetadata("starter_city");
    REQUIRE(cityMeta.has_value());
    REQUIRE(cityMeta->zoneType == "city");
}

TEST_CASE("ensureZoneGenerated is idempotent", "[worldgen]")
{
    tbeq::test::TempDatabase tempDb;
    REQUIRE(tempDb.generateWorld(42));

    tbeq::db::Database database(tempDb.path().string());
    REQUIRE(database.open());

    REQUIRE(tbeq::worldgen::ensureZoneGenerated(database, "starter_hunting", 42, TBEQ_DATA_ROOT));
    REQUIRE(database.hasZoneContent("starter_hunting"));
    REQUIRE(database.loadZoneSpawns("starter_hunting").size() == 5);

    REQUIRE(tbeq::worldgen::ensureZoneGenerated(database, "starter_hunting", 42, TBEQ_DATA_ROOT));
    REQUIRE(database.loadZoneSpawns("starter_hunting").size() == 5);
}

TEST_CASE("ZoneGenerator is deterministic for same seed and zone", "[worldgen]")
{
    tbeq::test::TempDatabase tempDb;
    REQUIRE(tempDb.generateWorld(42));

    tbeq::db::Database database(tempDb.path().string());
    REQUIRE(database.open());

    const auto first = generateZone(database, "starter_city", 42, TBEQ_DATA_ROOT);
    const auto second = generateZone(database, "starter_city", 42, TBEQ_DATA_ROOT);
    REQUIRE(tileHash(first) == tileHash(second));
    REQUIRE(first.portals.size() == second.portals.size());
    REQUIRE(first.portals.front().tileX == 32);
    REQUIRE(first.portals.front().tileY == 8);
}

TEST_CASE("portal pairing matches between city and hunting", "[worldgen]")
{
    tbeq::test::TempDatabase tempDb;
    REQUIRE(tempDb.generateWorld(42));

    tbeq::db::Database database(tempDb.path().string());
    REQUIRE(database.open());

    const auto city = generateZone(database, "starter_city", 42, TBEQ_DATA_ROOT);
    const auto hunting = generateZone(database, "starter_hunting", 42, TBEQ_DATA_ROOT);

    const auto cityPortal = city.portals.front();
    REQUIRE(cityPortal.destZoneId == "starter_hunting");
    REQUIRE(cityPortal.destX == 64);
    REQUIRE(cityPortal.destY == 64);

    const auto huntingSouth = hunting.portals[0];
    REQUIRE(huntingSouth.destZoneId == "starter_city");
    REQUIRE(huntingSouth.destX == 32);
    REQUIRE(huntingSouth.destY == 56);
}

TEST_CASE("PortalPlacementResolver agrees on cross-zone coordinates", "[worldgen]")
{
    const tbeq::worldgen::ZoneLink cityLink{
        "city_to_hunting",
        "starter_city",
        "starter_hunting",
        "north",
        "south",
        "To Hunting Grounds"};
    const auto cityPlacement = tbeq::worldgen::PortalPlacementResolver::resolve(cityLink, 42);
    REQUIRE(cityPlacement.has_value());
    REQUIRE(cityPlacement->destX == 64);
    REQUIRE(cityPlacement->destY == 64);

    const tbeq::worldgen::ZoneLink huntingLink{
        "hunting_to_city",
        "starter_hunting",
        "starter_city",
        "south",
        "north",
        "To Starter City"};
    const auto huntingPlacement = tbeq::worldgen::PortalPlacementResolver::resolve(huntingLink, 42);
    REQUIRE(huntingPlacement.has_value());
    REQUIRE(huntingPlacement->destX == 32);
    REQUIRE(huntingPlacement->destY == 56);
}

TEST_CASE("WorldLayoutGraph builds starter zones from templates", "[worldgen]")
{
    tbeq::worldgen::ZoneTypeCatalog catalog;
    REQUIRE(catalog.loadFromFile(TBEQ_DATA_ROOT / "worldgen" / "zone_templates.json"));

    const auto world = tbeq::worldgen::buildStarterWorldGraph(catalog);
    REQUIRE(world.zones.size() == 3);
    REQUIRE(world.links.size() == 4);
    REQUIRE(world.zones[0].zoneType == "city");
    REQUIRE(world.zones[0].width == 64);
    REQUIRE(world.zones[1].width == 128);
}
