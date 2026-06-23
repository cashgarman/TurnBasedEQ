#pragma once

#include <string>

namespace tbeq::auth
{

struct PasswordHashResult
{
    std::string hash;
    std::string salt;
};

PasswordHashResult hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt);

} // namespace tbeq::auth
