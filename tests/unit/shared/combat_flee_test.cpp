#include <catch2/catch_test_macros.hpp>

#include <random>

#include "tbeq/skills/SkillResolver.hpp"

TEST_CASE("flee success improves with defense and agility", "[combat]")
{
    tbeq::SkillResolver resolver;
    std::mt19937 lowRng(100);
    std::mt19937 highRng(100);

    int lowSuccess = 0;
    int highSuccess = 0;
    constexpr int kTrials = 300;

    for (int i = 0; i < kTrials; ++i)
    {
        if (resolver.rollFlee(5, 50, 2, lowRng))
        {
            ++lowSuccess;
        }
        if (resolver.rollFlee(80, 120, 2, highRng))
        {
            ++highSuccess;
        }
    }

    REQUIRE(highSuccess > lowSuccess);
}

TEST_CASE("flee chance drops with more enemies", "[combat]")
{
    tbeq::SkillResolver resolver;
    std::mt19937 fewRng(55);
    std::mt19937 manyRng(55);

    int fewSuccess = 0;
    int manySuccess = 0;
    constexpr int kTrials = 400;

    for (int i = 0; i < kTrials; ++i)
    {
        if (resolver.rollFlee(40, 80, 1, fewRng))
        {
            ++fewSuccess;
        }
        if (resolver.rollFlee(40, 80, 4, manyRng))
        {
            ++manySuccess;
        }
    }

    REQUIRE(fewSuccess > manySuccess);
}
