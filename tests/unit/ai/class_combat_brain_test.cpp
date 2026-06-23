#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "tbeq/ai/ClassCombatBrain.hpp"
#include "tbeq/ai/ClassCombatProfile.hpp"
#include "tbeq/combat/CombatTypes.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"

namespace
{

const std::filesystem::path kDataRoot = std::filesystem::path(TBEQ_REPO_ROOT) / "data";

} // namespace

TEST_CASE("ClassCombatBrain heals ally below threshold", "[ai]")
{
    tbeq::ai::ClassCombatProfileCatalog profiles;
    tbeq::content::SpellCatalog spells;
    tbeq::content::AbilityCatalog abilities;
    REQUIRE(profiles.loadFromFile(kDataRoot / "ai" / "class_combat_profiles.json"));
    REQUIRE(spells.loadFromFile(kDataRoot / "spells.json"));
    REQUIRE(abilities.loadFromFile(kDataRoot / "abilities.json"));

    tbeq::ai::ClassCombatBrain brain(profiles, spells, abilities);

    REQUIRE(brain.shouldHealAlly(0.50f, "cleric"));
    REQUIRE_FALSE(brain.shouldHealAlly(0.90f, "cleric"));

    tbeq::combat::CombatParticipant cleric;
    cleric.slot = 1;
    cleric.name = "Cleric Companion";
    cleric.classId = "cleric";
    cleric.side = tbeq::combat::CombatSide::Player;
    cleric.mana = 100;
    cleric.maxMana = 100;
    cleric.unlockedSpells = {"cleric_heal"};
    cleric.isAiCompanion = true;

    tbeq::combat::CombatParticipant warrior;
    warrior.slot = 2;
    warrior.name = "Warrior";
    warrior.classId = "warrior";
    warrior.side = tbeq::combat::CombatSide::Player;
    warrior.hp = 30;
    warrior.maxHp = 100;
    warrior.isAlive = true;

    tbeq::combat::CombatParticipant rat;
    rat.slot = 3;
    rat.name = "Rat";
    rat.side = tbeq::combat::CombatSide::Enemy;
    rat.hp = 40;
    rat.maxHp = 40;
    rat.isAlive = true;

    tbeq::ai::CombatBrainSnapshot snapshot;
    snapshot.actorSlot = cleric.slot;
    snapshot.participants = {cleric, warrior, rat};

    const auto intent = brain.chooseAction(cleric, snapshot);
    REQUIRE(intent.action == tbeq::combat::CombatActionType::CastSpell);
    REQUIRE(intent.spellId == "cleric_heal");
    REQUIRE(intent.targetSlot == warrior.slot);
}
