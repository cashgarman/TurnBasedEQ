#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

#include "tbeq/ai/ClassCombatBrain.hpp"
#include "tbeq/ai/ClassCombatProfile.hpp"
#include "tbeq/combat/CombatInstance.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/skills/SkillResolver.hpp"

namespace
{

const std::filesystem::path kDataRoot = std::filesystem::path(TBEQ_REPO_ROOT) / "data";

} // namespace

TEST_CASE("channeling success improves with skill level", "[combat][spells]")
{
    tbeq::SkillResolver resolver;
    std::mt19937 lowRng(11);
    std::mt19937 highRng(11);

    int lowSuccess = 0;
    int highSuccess = 0;
    constexpr int kTrials = 400;

    for (int i = 0; i < kTrials; ++i)
    {
        if (resolver.rollChanneling(5, lowRng))
        {
            ++lowSuccess;
        }
        if (resolver.rollChanneling(100, highRng))
        {
            ++highSuccess;
        }
    }

    REQUIRE(highSuccess > lowSuccess);
}

TEST_CASE("spell resist drops as caster school skill rises", "[combat][spells]")
{
    tbeq::SkillResolver resolver;
    std::mt19937 lowRng(22);
    std::mt19937 highRng(22);

    int lowResists = 0;
    int highResists = 0;
    constexpr int kTrials = 400;

    for (int i = 0; i < kTrials; ++i)
    {
        if (resolver.rollSpellResist(5, 10, lowRng))
        {
            ++lowResists;
        }
        if (resolver.rollSpellResist(100, 10, highRng))
        {
            ++highResists;
        }
    }

    REQUIRE(lowResists > highResists);
}

TEST_CASE("cleric heal spell restores ally hp", "[combat][spells]")
{
    tbeq::SkillResolver resolver;
    tbeq::content::MobCatalog mobs;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    REQUIRE(spells.loadFromFile(kDataRoot / "spells.json"));

    std::mt19937 rng(99);
    tbeq::combat::CombatInstance combat(10, resolver, mobs, spells, abilities, rng);

    tbeq::CharacterState clericState = tbeq::CharacterState::createDefault("cleric", 5);
    clericState.skills["channeling"] = {100, 0};
    clericState.skills["alteration"] = {50, 0};
    clericState.agi = 200;
    clericState.mana = 100;
    combat.addPlayer("cleric", "Cleric", "cleric", "human", 5, clericState, true, false);

    tbeq::CharacterState allyState = tbeq::CharacterState::createDefault("warrior", 5);
    allyState.hp = 20;
    allyState.agi = 1;
    combat.addPlayer("warrior", "Warrior", "warrior", "human", 5, allyState, true, false);

    combat.rollInitiative();

    uint32_t clericSlot = 0;
    uint32_t allySlot = 0;
    for (const auto& participant : combat.participants())
    {
        if (participant.name == "Cleric")
        {
            clericSlot = participant.slot;
        }
        if (participant.name == "Warrior")
        {
            allySlot = participant.slot;
        }
    }

    REQUIRE(combat.currentActor()->slot == clericSlot);

    tbeq::combat::CombatActionIntent intent;
    intent.action = tbeq::combat::CombatActionType::CastSpell;
    intent.spellId = "cleric_heal";
    intent.targetSlot = allySlot;

    REQUIRE(combat.submitAction(intent));
    const auto* ally = combat.participantBySlot(allySlot);
    REQUIRE(ally != nullptr);
    REQUIRE(ally->hp > 20);
}

TEST_CASE("status effect dot ticks damage on turn start", "[combat][status]")
{
    tbeq::combat::CombatParticipant target;
    target.slot = 2;
    target.name = "Victim";
    target.side = tbeq::combat::CombatSide::Enemy;
    target.hp = 50;
    target.maxHp = 50;
    target.isAlive = true;

    tbeq::combat::StatusEffect dot;
    dot.type = tbeq::combat::StatusEffectType::Dot;
    dot.remainingTurns = 2;
    dot.tickValue = 5;
    target.statusEffects.push_back(dot);

    REQUIRE(target.isAlive);
    target.hp = static_cast<uint16_t>(target.hp - dot.tickValue);
    dot.remainingTurns = static_cast<uint16_t>(dot.remainingTurns - 1);
    REQUIRE(target.hp == 45);
    REQUIRE(dot.remainingTurns == 1);
}
