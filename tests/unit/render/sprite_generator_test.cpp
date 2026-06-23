#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "procedural/SpriteGenerator.hpp"
#include "procedural/TileGenerator.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

TEST_CASE("sprite generator is deterministic", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto pixelsA = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    const auto pixelsB = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    REQUIRE(tbeq::render::hashPixels(pixelsA) == tbeq::render::hashPixels(pixelsB));
    REQUIRE(pixelsA.size()
        == static_cast<std::size_t>(tbeq::render::SpriteGenerator::kSpriteSize
            * tbeq::render::SpriteGenerator::kSpriteSize * 4));
}

TEST_CASE("sprite generator varies by race and class", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto humanWarrior = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    const auto elfWizard = generator.generateFrame(0, "wood_elf", "wizard", "", *style, spriteCatalog, 0);
    REQUIRE(tbeq::render::hashPixels(humanWarrior) != tbeq::render::hashPixels(elfWizard));
}

TEST_CASE("sprite generator npc variant differs from player", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto player = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    const auto merchant = generator.generateFrame(1, "", "", "merchant", *style, spriteCatalog, 0);
    const auto lorekeeper = generator.generateFrame(1, "", "", "lore", *style, spriteCatalog, 0);
    REQUIRE(tbeq::render::hashPixels(player) != tbeq::render::hashPixels(merchant));
    REQUIRE(tbeq::render::hashPixels(merchant) != tbeq::render::hashPixels(lorekeeper));
}

TEST_CASE("sprite generator walk bob alternates frames", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto frame0 = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    const auto frame1 = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 1);
    REQUIRE(tbeq::render::hashPixels(frame0) != tbeq::render::hashPixels(frame1));
}

TEST_CASE("sprite generator gear layers change appearance", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto bare = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0);
    tbeq::render::GearLayerTints gear;
    gear.weapon = "#c0a060";
    gear.head = "#6b4f2a";
    gear.chest = "#4a6b8a";
    gear.hands = "#808080";
    const auto geared = generator.generateFrame(0, "human", "warrior", "", *style, spriteCatalog, 0, gear);
    REQUIRE(tbeq::render::hashPixels(bare) != tbeq::render::hashPixels(geared));
}

TEST_CASE("entity sprite catalog loads from json", "[render][sprite]")
{
    tbeq::render::EntitySpriteCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    REQUIRE(catalog.raceTint("human").saturation == 1.0f);
    REQUIRE(catalog.npcRole("merchant").roleColor == "#c9a227");
}
