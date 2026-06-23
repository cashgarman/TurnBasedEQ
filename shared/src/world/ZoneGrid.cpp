#include "tbeq/world/ZoneGrid.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

namespace tbeq::world
{

namespace
{

bool isAdjacent(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY)
{
    const int32_t dx = std::abs(toX - fromX);
    const int32_t dy = std::abs(toY - fromY);
    return (dx + dy) == 1;
}

} // namespace

bool ZoneGrid::loadFromDatabase(
    const db::Database& database,
    const std::string& zoneId,
    const TileDefCatalog& tileDefs)
{
    tileDefs_ = &tileDefs;
    zoneId_ = zoneId;

    const auto metadata = database.loadZoneMetadata(zoneId);
    if (!metadata.has_value())
    {
        return false;
    }

    zoneName_ = metadata->name;
    tileStyle_ = metadata->tileStyle;
    width_ = metadata->width;
    height_ = metadata->height;
    isSafe_ = metadata->isSafe;

    const auto tileJson = database.loadZoneTiles(zoneId);
    if (!tileJson.has_value())
    {
        return false;
    }

    nlohmann::json json = nlohmann::json::parse(*tileJson);
    if (!json.is_array())
    {
        return false;
    }

    tiles_.clear();
    tiles_.reserve(json.size());
    for (const auto& tile : json)
    {
        tiles_.push_back(tile.get<std::string>());
    }

    if (static_cast<int32_t>(tiles_.size()) != width_ * height_)
    {
        return false;
    }

    portals_.clear();
    for (const auto& portal : database.loadZonePortals(zoneId))
    {
        portals_.push_back(ZonePortal{
            portal.tileX,
            portal.tileY,
            portal.destZoneId,
            portal.destX,
            portal.destY,
            portal.label});
    }

    return true;
}

std::string ZoneGrid::tileAt(int32_t x, int32_t y) const
{
    if (!inBounds(x, y))
    {
        return "stone_wall";
    }
    const std::size_t index = static_cast<std::size_t>(y * width_ + x);
    return tiles_[index];
}

bool ZoneGrid::inBounds(int32_t x, int32_t y) const
{
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool ZoneGrid::isWalkable(int32_t x, int32_t y) const
{
    if (!inBounds(x, y) || tileDefs_ == nullptr)
    {
        return false;
    }
    return tileDefs_->isWalkable(tileAt(x, y));
}

bool ZoneGrid::canStepTo(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY) const
{
    if (!isAdjacent(fromX, fromY, toX, toY))
    {
        return false;
    }
    return isWalkable(toX, toY);
}

std::optional<ZonePortal> ZoneGrid::portalAt(int32_t x, int32_t y) const
{
    for (const auto& portal : portals_)
    {
        if (portal.tileX == x && portal.tileY == y)
        {
            return portal;
        }
    }
    return std::nullopt;
}

} // namespace tbeq::world
