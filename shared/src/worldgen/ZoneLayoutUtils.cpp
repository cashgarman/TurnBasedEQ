#include "tbeq/worldgen/ZoneLayoutUtils.hpp"

#include <algorithm>

namespace tbeq::worldgen
{

std::vector<std::string> makeFilledGrid(int32_t width, int32_t height, const std::string& tileId)
{
    return std::vector<std::string>(static_cast<std::size_t>(width * height), tileId);
}

void setTile(std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y, const std::string& tileId)
{
    if (x < 0 || y < 0)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(y * width + x);
    if (index < tiles.size())
    {
        tiles[index] = tileId;
    }
}

std::string tileAt(const std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y)
{
    const std::size_t index = static_cast<std::size_t>(y * width + x);
    if (index >= tiles.size())
    {
        return "stone_wall";
    }
    return tiles[index];
}

void carveRect(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    const std::string& tileId)
{
    for (int32_t y = y0; y <= y1; ++y)
    {
        for (int32_t x = x0; x <= x1; ++x)
        {
            setTile(tiles, width, x, y, tileId);
        }
    }
}

void carveHorizontalPath(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t y,
    int32_t x0,
    int32_t x1,
    const std::string& tileId)
{
    const int32_t start = std::min(x0, x1);
    const int32_t end = std::max(x0, x1);
    for (int32_t x = start; x <= end; ++x)
    {
        setTile(tiles, width, x, y, tileId);
    }
}

void carveVerticalPath(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t x,
    int32_t y0,
    int32_t y1,
    const std::string& tileId)
{
    const int32_t start = std::min(y0, y1);
    const int32_t end = std::max(y0, y1);
    for (int32_t y = start; y <= end; ++y)
    {
        setTile(tiles, width, x, y, tileId);
    }
}

std::string tilesToJson(const std::vector<std::string>& tiles)
{
    return nlohmann::json(tiles).dump();
}

} // namespace tbeq::worldgen
