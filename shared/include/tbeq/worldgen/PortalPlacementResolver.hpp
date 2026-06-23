#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "tbeq/worldgen/WorldGenTypes.hpp"

namespace tbeq::worldgen
{

struct ResolvedPortal
{
    int32_t tileX = 0;
    int32_t tileY = 0;
    int32_t destX = 0;
    int32_t destY = 0;
};

class PortalPlacementResolver
{
public:
    static std::optional<ResolvedPortal> resolve(
        const ZoneLink& link,
        int64_t worldSeed);
};

} // namespace tbeq::worldgen
