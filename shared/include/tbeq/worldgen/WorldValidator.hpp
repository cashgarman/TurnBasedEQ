#pragma once

#include <filesystem>
#include <string>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenTypes.hpp"

namespace tbeq::worldgen
{

class WorldValidator
{
public:
    explicit WorldValidator(ValidatorRules rules);

    ValidationReport validate(const GeneratedWorld& world, const std::filesystem::path& dataRoot) const;

private:
    ValidatorRules rules_;
};

} // namespace tbeq::worldgen
