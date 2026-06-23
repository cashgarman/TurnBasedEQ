#include <catch2/catch_test_macros.hpp>

#include "tbeq/auth/CharacterValidation.hpp"
#include "tbeq/auth/PasswordHash.hpp"

#include <nlohmann/json.hpp>

TEST_CASE("auth validates usernames and passwords", "[auth]")
{
    const auto validUsername = tbeq::auth::validateUsername("player_one");
    REQUIRE(validUsername.ok);

    const auto shortUsername = tbeq::auth::validateUsername("ab");
    REQUIRE_FALSE(shortUsername.ok);

    const auto validPassword = tbeq::auth::validatePassword("secret123");
    REQUIRE(validPassword.ok);

    const auto shortPassword = tbeq::auth::validatePassword("123");
    REQUIRE_FALSE(shortPassword.ok);
}

TEST_CASE("auth hashes and verifies passwords", "[auth]")
{
    const auto hashed = tbeq::auth::hashPassword("test-password");
    REQUIRE_FALSE(hashed.hash.empty());
    REQUIRE_FALSE(hashed.salt.empty());
    REQUIRE(tbeq::auth::verifyPassword("test-password", hashed.hash, hashed.salt));
    REQUIRE_FALSE(tbeq::auth::verifyPassword("wrong-password", hashed.hash, hashed.salt));
}

TEST_CASE("auth validates character creation against race/class tables", "[auth]")
{
    const nlohmann::json races = nlohmann::json::parse(R"([
        {"id":"human","allowedClasses":["warrior","wizard"]}
    ])");
    const nlohmann::json classes = nlohmann::json::parse(R"([
        {"id":"warrior"},{"id":"wizard"}
    ])");

    const auto valid = tbeq::auth::validateCharacterCreate("Hero", "human", "wizard", races, classes);
    REQUIRE(valid.ok);

    const auto invalidClass = tbeq::auth::validateCharacterCreate("Hero", "human", "rogue", races, classes);
    REQUIRE_FALSE(invalidClass.ok);
}
