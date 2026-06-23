#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tbeq/persistence/Database.hpp"
#include "tbeq/world/TileDefCatalog.hpp"

namespace tbeq::world
{

struct ZonePortal
{
    int32_t tileX = 0;
    int32_t tileY = 0;
    std::string destZoneId;
    int32_t destX = 0;
    int32_t destY = 0;
    std::string label;
};

class ZoneGrid
{
public:
    bool loadFromDatabase(
        const db::Database& database,
        const std::string& zoneId,
        const TileDefCatalog& tileDefs);

    const std::string& zoneId() const { return zoneId_; }
    const std::string& zoneName() const { return zoneName_; }
    const std::string& tileStyle() const { return tileStyle_; }
    int32_t width() const { return width_; }
    int32_t height() const { return height_; }
    bool isSafe() const { return isSafe_; }

    const std::vector<std::string>& tiles() const { return tiles_; }
    const std::vector<ZonePortal>& portals() const { return portals_; }

    std::string tileAt(int32_t x, int32_t y) const;
    bool inBounds(int32_t x, int32_t y) const;
    bool isWalkable(int32_t x, int32_t y) const;
    bool canStepTo(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY) const;
    std::optional<ZonePortal> portalAt(int32_t x, int32_t y) const;

private:
    std::string zoneId_;
    std::string zoneName_;
    std::string tileStyle_;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool isSafe_ = false;
    std::vector<std::string> tiles_;
    std::vector<ZonePortal> portals_;
    const TileDefCatalog* tileDefs_ = nullptr;
};

} // namespace tbeq::world
