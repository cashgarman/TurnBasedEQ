#include <catch2/catch_test_macros.hpp>

#include <random>

#include "tbeq/combat/BossScriptCatalog.hpp"
#include "tbeq/combat/CombatInstance.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/skills/SkillResolver.hpp"

TEST_CASE("combat initiative orders higher agility first with fixed rng", "[combat]")
{
    tbeq::SkillResolver resolver;
    tbeq::content::MobCatalog catalog;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    tbeq::combat::BossScriptCatalog bossScripts;
    std::mt19937 rng(42);

    tbeq::combat::CombatInstance combat(1, resolver, catalog, spells, abilities, bossScripts, rng);
    tbeq::CharacterState fastState = tbeq::CharacterState::createDefault("warrior", 1);
    fastState.agi = 100;
    tbeq::CharacterState slowState = tbeq::CharacterState::createDefault("warrior", 1);
    slowState.agi = 10;

    combat.addPlayer("fast", "Fast", "warrior", "human", 1, fastState, true, false);
    combat.addPlayer("slow", "Slow", "warrior", "human", 1, slowState, true, false);
    combat.rollInitiative();

    REQUIRE(combat.turnOrder().size() == 2);
    const auto* first = combat.participantBySlot(combat.turnOrder().front());
    REQUIRE(first != nullptr);
    REQUIRE(first->name == "Fast");
}

TEST_CASE("combat initiative includes all living participants", "[combat]")
{
    tbeq::SkillResolver resolver;
    tbeq::content::MobCatalog catalog;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    tbeq::combat::BossScriptCatalog bossScripts;
    std::mt19937 rng(7);

    tbeq::combat::CombatInstance combat(2, resolver, catalog, spells, abilities, bossScripts, rng);
    tbeq::CharacterState playerState = tbeq::CharacterState::createDefault("warrior", 1);
    combat.addPlayer("hero", "Hero", "warrior", "human", 1, playerState, true, false);

    tbeq::content::MobDef mob;
    mob.id = "test_rat";
    mob.name = "Rat";
    mob.hp = 20;
    mob.agi = 50;
    combat.addMob(mob);

    combat.rollInitiative();
    REQUIRE(combat.turnOrder().size() == 2);
}
