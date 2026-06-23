#include <catch2/catch_test_macros.hpp>

#include <random>

#include "tbeq/combat/BossScriptCatalog.hpp"
#include "tbeq/combat/CombatInstance.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/skills/SkillResolver.hpp"

TEST_CASE("melee hit chance rises with offense skill", "[combat]")
{
    tbeq::SkillResolver resolver;
    std::mt19937 rng(12345);

    int lowHits = 0;
    int highHits = 0;
    constexpr int kTrials = 500;

    for (int i = 0; i < kTrials; ++i)
    {
        if (resolver.rollMeleeHit(5, 5, 20, rng))
        {
            ++lowHits;
        }
        if (resolver.rollMeleeHit(50, 50, 20, rng))
        {
            ++highHits;
        }
    }

    REQUIRE(highHits > lowHits);
}

TEST_CASE("combat melee action can damage enemy", "[combat]")
{
    tbeq::SkillResolver resolver;
    tbeq::content::MobCatalog catalog;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    tbeq::combat::BossScriptCatalog bossScripts;
    std::mt19937 rng(999);

    tbeq::combat::CombatInstance combat(3, resolver, catalog, spells, abilities, bossScripts, rng);
    tbeq::CharacterState playerState = tbeq::CharacterState::createDefault("warrior", 5);
    playerState.skills["offense"] = {40, 0};
    playerState.skills["1h_slash"] = {40, 0};
    combat.addPlayer("hero", "Hero", "warrior", "human", 5, playerState, true, false);

    tbeq::content::MobDef mob;
    mob.id = "dummy";
    mob.name = "Dummy";
    mob.hp = 200;
    mob.defense = 5;
    mob.agi = 1;
    combat.addMob(mob);

    combat.rollInitiative();

    const uint32_t playerSlot = combat.turnOrder().front();
    const uint32_t enemySlot = combat.turnOrder().back();
    REQUIRE(combat.submitAction(playerSlot, tbeq::combat::CombatActionType::MeleeAttack, enemySlot));

    const auto* enemy = combat.participantBySlot(enemySlot);
    REQUIRE(enemy != nullptr);
    REQUIRE(enemy->hp < enemy->maxHp);
}

TEST_CASE("melee attack can miss against high defense", "[combat]")
{
    tbeq::SkillResolver resolver;
    tbeq::content::MobCatalog catalog;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    tbeq::combat::BossScriptCatalog bossScripts;
    std::mt19937 rng(2024);

    bool sawMiss = false;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        tbeq::combat::CombatInstance combat(static_cast<uint32_t>(attempt + 10), resolver, catalog, spells, abilities, bossScripts, rng);
        tbeq::CharacterState playerState = tbeq::CharacterState::createDefault("warrior", 1);
        playerState.skills["offense"] = {1, 0};
        playerState.skills["1h_slash"] = {1, 0};
        combat.addPlayer("hero", "Hero", "warrior", "human", 1, playerState, true, false);

        tbeq::content::MobDef mob;
        mob.id = "turtle";
        mob.name = "Turtle";
        mob.hp = 500;
        mob.defense = 200;
        mob.agi = 1;
        combat.addMob(mob);
        combat.rollInitiative();

        const uint32_t playerSlot = combat.turnOrder().front();
        const uint32_t enemySlot = combat.turnOrder().back();
        combat.submitAction(playerSlot, tbeq::combat::CombatActionType::MeleeAttack, enemySlot);
        for (const auto& event : combat.takePendingEvents())
        {
            if (event.type == tbeq::combat::CombatEventType::Miss)
            {
                sawMiss = true;
                break;
            }
        }
        if (sawMiss)
        {
            break;
        }
    }

    REQUIRE(sawMiss);
}
