#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq::worldgen
{

struct GeneratedZone
{
    std::string id;
    std::string name;
    std::string role;
    std::string biome;
    std::string tileStyle;
    int32_t width = 0;
    int32_t height = 0;
    bool isSafe = false;
    std::vector<std::string> tiles;
    struct Portal
    {
        int32_t tileX = 0;
        int32_t tileY = 0;
        std::string destZoneId;
        int32_t destX = 0;
        int32_t destY = 0;
        std::string label;
    };
    std::vector<Portal> portals;
    struct Spawn
    {
        std::string spawnId;
        int32_t tileX = 0;
        int32_t tileY = 0;
        std::string mobTable;
        int32_t respawnSeconds = 60;
    };
    std::vector<Spawn> spawns;
    struct NpcSlot
    {
        std::string slotType;
        int32_t tileX = 0;
        int32_t tileY = 0;
        std::string npcId;
    };
    std::vector<NpcSlot> npcSlots;
};

struct GeneratedWorld
{
    int64_t seed = 0;
    std::string version;
    std::vector<GeneratedZone> zones;
};

struct ValidatorRules
{
    bool requireConnectedGraph = true;
    double minWalkableRatio = 0.35;
    int32_t maxSingleTileChokeCount = 4;
};

struct ValidationReport
{
    bool ok = false;
    std::vector<std::string> errors;
};

} // namespace tbeq::worldgen
