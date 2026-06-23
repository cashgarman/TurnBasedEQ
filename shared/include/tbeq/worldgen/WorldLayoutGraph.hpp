#pragma once

#include "tbeq/worldgen/WorldGenTypes.hpp"

namespace tbeq::worldgen
{

class ZoneTypeCatalog;

GeneratedWorld buildStarterWorldGraph(const ZoneTypeCatalog& catalog);

} // namespace tbeq::worldgen
