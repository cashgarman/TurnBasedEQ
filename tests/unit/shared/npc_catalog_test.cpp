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
