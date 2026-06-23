#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tbeq::worldgen
{

std::vector<std::string> makeFilledGrid(int32_t width, int32_t height, const std::string& tileId);
void setTile(std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y, const std::string& tileId);
std::string tileAt(const std::vector<std::string>& tiles, int32_t width, int32_t x, int32_t y);
void carveRect(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    const std::string& tileId);
void carveHorizontalPath(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t y,
    int32_t x0,
    int32_t x1,
    const std::string& tileId);
void carveVerticalPath(
    std::vector<std::string>& tiles,
    int32_t width,
    int32_t x,
    int32_t y0,
    int32_t y1,
    const std::string& tileId);
std::string tilesToJson(const std::vector<std::string>& tiles);

} // namespace tbeq::worldgen
