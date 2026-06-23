#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/items/ItemRules.hpp"

#ifdef TBEQ_REPO_ROOT
#define TBEQ_DATA_ROOT TBEQ_REPO_ROOT "/data"
#else
#define TBEQ_DATA_ROOT "data"
#endif

TEST_CASE("equip rules reject wrong class", "[items]")
{
    tbeq::content::ItemDef item;
    item.id = "rusty_dagger";
    item.slot = tbeq::content::ItemSlot::Weapon;
    item.allowedClasses = {"warrior", "rogue"};

    tbeq::items::EquipContext context;
    context.classId = "wizard";
    context.raceId = "human";
    context.level = 5;

    const tbeq::CharacterState state = tbeq::CharacterState::createDefault("wizard", 5);
    const auto result = tbeq::items::ItemRules::canEquip(item, context, state);
    REQUIRE(!result.ok);
    REQUIRE(result.message == "Class cannot equip this item.");
}

TEST_CASE("equip rules accept valid warrior weapon", "[items]")
{
    tbeq::content::ItemDef item;
    item.id = "rusty_dagger";
    item.slot = tbeq::content::ItemSlot::Weapon;
    item.allowedClasses = {"warrior", "rogue"};
    item.requiredLevel = 1;

    tbeq::items::EquipContext context;
    context.classId = "warrior";
    context.raceId = "human";
    context.level = 1;

    const tbeq::CharacterState state = tbeq::CharacterState::createDefault("warrior", 1);
    const auto result = tbeq::items::ItemRules::canEquip(item, context, state);
    REQUIRE(result.ok);
}

TEST_CASE("recompute derived stats applies weapon hp bonus", "[items]")
{
    tbeq::CharacterState state = tbeq::CharacterState::createDefault("warrior", 3);
    const uint16_t baselineMaxHp = state.maxHp;
    state.equipment["weapon"] = "chief_blade";

    tbeq::content::ItemCatalog loadedCatalog;
    REQUIRE(loadedCatalog.loadFromFile(std::filesystem::path(TBEQ_DATA_ROOT) / "items.json"));
    tbeq::items::ItemRules::recomputeDerivedStats(state, loadedCatalog);

    const tbeq::content::ItemDef* loadedWeapon = loadedCatalog.findItem("chief_blade");
    REQUIRE(loadedWeapon != nullptr);
    REQUIRE(state.maxHp == baselineMaxHp + loadedWeapon->stats.hp);
    REQUIRE(state.equippedWeaponSkill == loadedWeapon->weaponSkill);
}
