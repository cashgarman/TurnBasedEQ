#include <catch2/catch_test_macros.hpp>

#include "tbeq/content/NpcCatalog.hpp"

TEST_CASE("npc catalog resolves legacy merchant ids", "[content]")
{
    tbeq::content::NpcCatalog catalog;
    REQUIRE(catalog.loadFromFile(TBEQ_REPO_ROOT "/data/npcs.json"));

    const tbeq::content::NpcDef* merchant = catalog.findNpc("merchant_starter_1");
    REQUIRE(merchant != nullptr);
    REQUIRE(merchant->id == "starter_city_merchant_1");
    REQUIRE(merchant->name == "Grobb Provisioner");
    REQUIRE(catalog.resolveNpcId("merchant_starter_2") == "starter_city_merchant_2");
}

TEST_CASE("npc catalog resolves lorekeeper ids and lore lines", "[content]")
{
    tbeq::content::NpcCatalog catalog;
    REQUIRE(catalog.loadFromFile(TBEQ_REPO_ROOT "/data/npcs.json"));

    const tbeq::content::NpcDef* lorekeeper = catalog.findNpc("lorekeeper_starter_2");
    REQUIRE(lorekeeper != nullptr);
    REQUIRE(lorekeeper->id == "starter_city_lorekeeper_2");
    REQUIRE(lorekeeper->name == "Historian Vale");
    REQUIRE(!lorekeeper->loreLines.empty());
    REQUIRE(catalog.resolveNpcId("lorekeeper_starter_3") == "starter_city_lorekeeper_3");
}
