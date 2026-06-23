#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "procedural/SpriteGenerator.hpp"
#include "procedural/TileGenerator.hpp"
#include "render/AnimationTypes.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

namespace
{

const auto kIdleSouth0 = tbeq::render::AnimationClipId::Idle;
const auto kFacingSouth = tbeq::render::Facing::South;
const auto kFacingEast = tbeq::render::Facing::East;
const auto kWalkSouth1 = tbeq::render::AnimationClipId::Walk;

} // namespace

TEST_CASE("sprite generator is deterministic", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto pixelsA = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto pixelsB = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
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
    const auto humanWarrior = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto elfWizard = generator.generateFrame(
        0, "wood_elf", "wizard", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
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
    const auto player = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto merchant = generator.generateFrame(
        1, "", "", "merchant", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto lorekeeper = generator.generateFrame(
        1, "", "", "lore", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    REQUIRE(tbeq::render::hashPixels(player) != tbeq::render::hashPixels(merchant));
    REQUIRE(tbeq::render::hashPixels(merchant) != tbeq::render::hashPixels(lorekeeper));
}

TEST_CASE("sprite generator walk frames differ from idle", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto idle = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto walk = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kWalkSouth1, kFacingSouth, 1);
    REQUIRE(tbeq::render::hashPixels(idle) != tbeq::render::hashPixels(walk));
}

TEST_CASE("sprite generator facing changes appearance", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto south = generator.generateFrame(
        2, "", "", "forest_boar", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto east = generator.generateFrame(
        2, "", "", "forest_boar", *style, spriteCatalog, kIdleSouth0, kFacingEast, 0);
    REQUIRE(tbeq::render::hashPixels(south) != tbeq::render::hashPixels(east));
}

TEST_CASE("sprite generator mob bodies differ by mob id", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto rat = generator.generateFrame(
        2, "", "", "forest_rat", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto boar = generator.generateFrame(
        2, "", "", "forest_boar", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto goblin = generator.generateFrame(
        2, "", "", "goblin_scout", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    REQUIRE(tbeq::render::hashPixels(rat) != tbeq::render::hashPixels(boar));
    REQUIRE(tbeq::render::hashPixels(boar) != tbeq::render::hashPixels(goblin));
}

TEST_CASE("mob overworld and combat share same sprite hash", "[render][sprite]")
{
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog spriteCatalog;
    REQUIRE(styleCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "tile_styles.json"));
    REQUIRE(spriteCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    const auto* style = styleCatalog.find("starter_city");
    REQUIRE(style != nullptr);

    tbeq::render::SpriteGenerator generator;
    const auto overworld = generator.generateFrame(
        2, "", "", "forest_rat", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    const auto combat = generator.generateFrame(
        2, "", "", "forest_rat", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    REQUIRE(tbeq::render::hashPixels(overworld) == tbeq::render::hashPixels(combat));
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
    const auto bare = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0);
    tbeq::render::GearLayerTints gear;
    gear.weapon = "#c0a060";
    gear.head = "#6b4f2a";
    gear.chest = "#4a6b8a";
    gear.hands = "#808080";
    const auto geared = generator.generateFrame(
        0, "human", "warrior", "", *style, spriteCatalog, kIdleSouth0, kFacingSouth, 0, gear);
    REQUIRE(tbeq::render::hashPixels(bare) != tbeq::render::hashPixels(geared));
}

TEST_CASE("entity sprite catalog loads mob bodies", "[render][sprite]")
{
    tbeq::render::EntitySpriteCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "entity_sprites.json"));
    REQUIRE(catalog.raceTint("human").saturation == 1.0f);
    REQUIRE(catalog.npcRole("merchant").roleColor == "#c9a227");
    REQUIRE(catalog.mobBody("forest_rat").silhouette == "quadruped_small");
}
