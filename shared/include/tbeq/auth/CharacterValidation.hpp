#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tbeq::auth
{

struct ValidationResult
{
    bool ok = false;
    std::string message;
};

ValidationResult validateUsername(const std::string& username);
ValidationResult validatePassword(const std::string& password);
ValidationResult validateCharacterName(const std::string& name);
ValidationResult validateCharacterCreate(
    const std::string& name,
    const std::string& raceId,
    const std::string& classId,
    const nlohmann::json& racesJson,
    const nlohmann::json& classesJson);

} // namespace tbeq::auth
