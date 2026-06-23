#include "tbeq/auth/CharacterValidation.hpp"

#include <algorithm>
#include <cctype>

namespace tbeq::auth
{

namespace
{

bool isAlphaNumeric(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) != 0;
}

bool containsOnlyAllowedNameChars(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    for (char ch : value)
    {
        if (!isAlphaNumeric(ch) && ch != '_' && ch != ' ')
        {
            return false;
        }
    }
    return true;
}

const nlohmann::json* findById(const nlohmann::json& entries, const std::string& id)
{
    if (!entries.is_array())
    {
        return nullptr;
    }

    for (const auto& entry : entries)
    {
        if (entry.contains("id") && entry["id"].get<std::string>() == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

ValidationResult validateUsername(const std::string& username)
{
    if (username.size() < 3 || username.size() > 32)
    {
        return {false, "Username must be 3-32 characters"};
    }

    for (char ch : username)
    {
        if (!isAlphaNumeric(ch) && ch != '_')
        {
            return {false, "Username may only contain letters, numbers, and underscore"};
        }
    }

    return {true, "ok"};
}

ValidationResult validatePassword(const std::string& password)
{
    if (password.size() < 6 || password.size() > 64)
    {
        return {false, "Password must be 6-64 characters"};
    }
    return {true, "ok"};
}

ValidationResult validateCharacterName(const std::string& name)
{
    if (name.size() < 3 || name.size() > 20)
    {
        return {false, "Character name must be 3-20 characters"};
    }

    if (!containsOnlyAllowedNameChars(name))
    {
        return {false, "Character name contains invalid characters"};
    }

    return {true, "ok"};
}

ValidationResult validateCharacterCreate(
    const std::string& name,
    const std::string& raceId,
    const std::string& classId,
    const nlohmann::json& racesJson,
    const nlohmann::json& classesJson)
{
    const auto nameResult = validateCharacterName(name);
    if (!nameResult.ok)
    {
        return nameResult;
    }

    const nlohmann::json* race = findById(racesJson, raceId);
    if (race == nullptr)
    {
        return {false, "Unknown race"};
    }

    const nlohmann::json* classDef = findById(classesJson, classId);
    if (classDef == nullptr)
    {
        return {false, "Unknown class"};
    }

    if (!race->contains("allowedClasses") || !(*race)["allowedClasses"].is_array())
    {
        return {false, "Race has no allowed classes"};
    }

    const auto& allowed = (*race)["allowedClasses"];
    const bool classAllowed = std::any_of(
        allowed.begin(),
        allowed.end(),
        [&classId](const nlohmann::json& entry)
        {
            return entry.get<std::string>() == classId;
        });

    if (!classAllowed)
    {
        return {false, "Class not allowed for race"};
    }

    return {true, "ok"};
}

} // namespace tbeq::auth
