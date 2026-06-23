#pragma once

#include <filesystem>
#include <string>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenTypes.hpp"

namespace tbeq::worldgen
{

class WorldGenerator
{
public:
    WorldGenerator(int64_t seed, const std::filesystem::path& dataRoot);

    GeneratedWorld generate() const;
    GeneratedWorld buildWorldSkeleton() const;
    bool writeWorldSkeletonToDatabase(db::Database& database) const;
    bool writeToDatabase(db::Database& database) const;
    ValidationReport validateOnly() const;
    ValidationReport validateSkeletonOnly() const;

private:
    int64_t seed_;
    std::filesystem::path dataRoot_;
};

ValidatorRules loadValidatorRules(const std::filesystem::path& dataRoot);

} // namespace tbeq::worldgen
