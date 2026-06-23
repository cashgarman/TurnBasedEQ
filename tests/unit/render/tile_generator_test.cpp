#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "procedural/TileGenerator.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

TEST_CASE("tile generator is deterministic", "[render][tile]")
{
    tbeq::render::TileStyleCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    const auto* style = catalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::TileGenerator generator;
    const auto pixelsA = generator.generateFrame("grass", "grass", *style, 4, 7, 15, 0, 4);
    const auto pixelsB = generator.generateFrame("grass", "grass", *style, 4, 7, 15, 0, 4);
    REQUIRE(tbeq::render::hashPixels(pixelsA) == tbeq::render::hashPixels(pixelsB));
    REQUIRE(pixelsA.size() == static_cast<std::size_t>(tbeq::render::TileGenerator::kTileSize * tbeq::render::TileGenerator::kTileSize * 4));
}

TEST_CASE("tile generator varies by coordinate", "[render][tile]")
{
    tbeq::render::TileStyleCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    const auto* style = catalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::TileGenerator generator;
    const auto pixelsA = generator.generateFrame("grass", "grass", *style, 1, 1, 15, 0, 1);
    const auto pixelsB = generator.generateFrame("grass", "grass", *style, 2, 2, 15, 0, 1);
    REQUIRE(tbeq::render::hashPixels(pixelsA) != tbeq::render::hashPixels(pixelsB));
}

TEST_CASE("water tiles animate across frames", "[render][tile]")
{
    tbeq::render::TileStyleCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    const auto* style = catalog.find("hunting_grounds");
    REQUIRE(style != nullptr);

    tbeq::render::TileGenerator generator;
    const auto frame0 = generator.generateFrame("shallow_water", "water", *style, 3, 3, 0, 0, 8);
    const auto frame1 = generator.generateFrame("shallow_water", "water", *style, 3, 3, 0, 1, 8);
    REQUIRE(tbeq::render::hashPixels(frame0) != tbeq::render::hashPixels(frame1));
}

TEST_CASE("autotile bitmask maps cardinal neighbors", "[render][autotile]")
{
    const uint32_t isolated = tbeq::render::resolveAutotileVariant("grass", false, false, false, false);
    const uint32_t connected = tbeq::render::resolveAutotileVariant("grass", true, true, true, true);
    REQUIRE(isolated == 0);
    REQUIRE(connected == 15);
}
