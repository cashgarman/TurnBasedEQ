#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenTypes.hpp"

namespace tbeq::worldgen
{

class ZoneTypeCatalog;

class ZoneGenerator
{
public:
    ZoneGenerator(int64_t worldSeed, const ZoneTypeCatalog& catalog);

    GeneratedZone generate(
        const db::ZoneMetadataRecord& metadata,
        const std::vector<ZoneLink>& outgoingLinks) const;

private:
    GeneratedZone generateCity(const db::ZoneMetadataRecord& metadata, const std::vector<ZoneLink>& outgoingLinks) const;
    GeneratedZone generateHunting(const db::ZoneMetadataRecord& metadata, const std::vector<ZoneLink>& outgoingLinks) const;
    GeneratedZone generateDungeon(const db::ZoneMetadataRecord& metadata, const std::vector<ZoneLink>& outgoingLinks) const;
    void applyPortals(
        GeneratedZone& zone,
        const std::vector<ZoneLink>& outgoingLinks) const;

    int64_t worldSeed_;
    const ZoneTypeCatalog& catalog_;
};

bool persistGeneratedZone(db::Database& database, const GeneratedZone& zone);

} // namespace tbeq::worldgen
