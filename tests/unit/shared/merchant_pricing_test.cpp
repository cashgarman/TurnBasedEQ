#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/items/ItemRules.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

TEST_CASE("merchant buy price decreases with merchant skill", "[items]")
{
    tbeq::content::ItemCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "items.json"));

    const tbeq::content::ItemDef* item = catalog.findItem("rusty_dagger");
    REQUIRE(item != nullptr);

    const uint32_t lowSkillPrice = tbeq::items::ItemRules::merchantBuyPrice(*item, 0, 100, 18);
    const uint32_t highSkillPrice = tbeq::items::ItemRules::merchantBuyPrice(*item, 100, 100, 18);
    REQUIRE(highSkillPrice < lowSkillPrice);
    REQUIRE(lowSkillPrice >= 1);
}

TEST_CASE("merchant sell price increases with merchant skill", "[items]")
{
    tbeq::content::ItemCatalog catalog;
    REQUIRE(catalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "items.json"));

    const tbeq::content::ItemDef* item = catalog.findItem("chief_blade");
    REQUIRE(item != nullptr);

    const uint32_t lowSkillPrice = tbeq::items::ItemRules::merchantSellPrice(*item, 0, 100);
    const uint32_t highSkillPrice = tbeq::items::ItemRules::merchantSellPrice(*item, 100, 100);
    REQUIRE(highSkillPrice > lowSkillPrice);
    REQUIRE(lowSkillPrice >= 1);
}
