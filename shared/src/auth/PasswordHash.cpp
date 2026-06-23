#include "tbeq/auth/PasswordHash.hpp"

#include <random>
#include <sstream>
#include <iomanip>

namespace tbeq::auth
{

namespace
{

std::string randomSalt()
{
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(0, 255);

    std::ostringstream stream;
    for (int i = 0; i < 16; ++i)
    {
        stream << std::hex << std::setw(2) << std::setfill('0') << distribution(generator);
    }
    return stream.str();
}

uint64_t fnv1a64(const std::string& value)
{
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : value)
    {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string deriveHash(const std::string& password, const std::string& salt, int iterations)
{
    std::string material = salt + password;
    uint64_t hash = fnv1a64(material);
    for (int i = 0; i < iterations; ++i)
    {
        material = std::to_string(hash) + salt + password;
        hash = fnv1a64(material);
    }

    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

} // namespace

PasswordHashResult hashPassword(const std::string& password)
{
    PasswordHashResult result;
    result.salt = randomSalt();
    result.hash = deriveHash(password, result.salt, 12000);
    return result;
}

bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt)
{
    return deriveHash(password, salt, 12000) == hash;
}

} // namespace tbeq::auth
