#include <catch2/catch_test_macros.hpp>

#include "tbeq/content/SkillCapCatalog.hpp"
#include "tbeq/skills/Progression.hpp"
#include "tbeq/skills/SkillResolver.hpp"

TEST_CASE("Character XP curve thresholds", "[shared][progression]")
{
    REQUIRE(tbeq::progression::experienceRequiredForLevel(1) == 0);
    REQUIRE(tbeq::progression::experienceRequiredForLevel(2) == 200);
    REQUIRE(tbeq::progression::experienceRequiredForLevel(3) == 450);
}

TEST_CASE("Character level-up grants HP and raises level", "[shared][progression]")
{
    tbeq::CharacterState state = tbeq::CharacterState::createDefault("warrior", 1);
    state.experience = 0;
    const auto result = tbeq::progression::grantCharacterExperience(state, 200);
    REQUIRE(result.leveledUp);
    REQUIRE(state.level == 2);
    REQUIRE(state.maxHp == 100);
}

TEST_CASE("Skill cap rises with character level", "[shared][progression]")
{
    tbeq::content::SkillCapCatalog caps;
    REQUIRE(caps.loadFromFile(TBEQ_REPO_ROOT "/data/skill_caps.json"));

    tbeq::SkillResolver resolver;
    resolver.setSkillCapCatalog(&caps);

    const uint16_t level1Cap = resolver.getCap("warrior", "offense", 1);
    const uint16_t level5Cap = resolver.getCap("warrior", "offense", 5);
    REQUIRE(level1Cap == 5);
    REQUIRE(level5Cap == 25);
    REQUIRE(level5Cap > level1Cap);
}

TEST_CASE("Skill XP thresholds and level-up at cap", "[shared][progression]")
{
    tbeq::SkillResolver resolver;
    tbeq::SkillProgress progress{1, 0};

    REQUIRE(tbeq::progression::skillExperienceToLevelUp(1) == 75);
    REQUIRE(resolver.applyGain(progress, 75, 10));
    REQUIRE(progress.level == 2);
    REQUIRE(progress.experience == 0);

    progress.level = 1;
    progress.experience = 0;
    REQUIRE(resolver.applyGain(progress, 10, 10));
    REQUIRE(progress.level == 1);
    REQUIRE(progress.experience == 10);

    progress.level = 10;
    progress.experience = 0;
    REQUIRE_FALSE(resolver.applyGain(progress, 100, 10));
    REQUIRE(progress.level == 10);
}

TEST_CASE("Mob XP reward scales for named and boss mobs", "[shared][progression]")
{
    const uint32_t normal = tbeq::progression::mobExperienceReward(2, 40, false, false);
    const uint32_t named = tbeq::progression::mobExperienceReward(2, 40, true, false);
    const uint32_t boss = tbeq::progression::mobExperienceReward(4, 90, false, true);
    REQUIRE(named > normal);
    REQUIRE(boss > normal);
}
