#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace tbeq::worldgen
{

struct CityTemplate
{
    int32_t width = 64;
    int32_t height = 64;
    int32_t merchantSlots = 2;
    int32_t lorekeeperSlots = 3;
};

struct HuntingTemplate
{
    int32_t width = 128;
    int32_t height = 128;
    int32_t spawnCampCount = 4;
};

struct DungeonTemplate
{
    int32_t width = 48;
    int32_t height = 48;
    int32_t roomCount = 12;
    int32_t bossRoomDepth = 8;
};

struct ZoneTypeDefaults
{
    std::string biome;
    std::string tileStyle;
    bool isSafe = false;
};

class ZoneTypeCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    std::optional<CityTemplate> cityTemplate() const;
    std::optional<HuntingTemplate> huntingTemplate() const;
    std::optional<DungeonTemplate> dungeonTemplate() const;
    std::optional<ZoneTypeDefaults> defaultsForType(const std::string& zoneType) const;

private:
    std::optional<CityTemplate> city_;
    std::optional<HuntingTemplate> hunting_;
    std::optional<DungeonTemplate> dungeon_;
    std::unordered_map<std::string, ZoneTypeDefaults> defaults_;
};

} // namespace tbeq::worldgen
