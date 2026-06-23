#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tbeq::render
{

struct TileStyleProfile
{
    std::string id;
    int64_t masterSeed = 0;
    std::string baseColor;
    std::string accentColor;
    std::string shadowColor;
    std::string highlightColor;
    float noiseScale = 0.15f;
    float hueShift = 0.0f;
    float saturation = 1.0f;
};

struct TileStyleCatalog
{
    bool loadFromFile(const std::filesystem::path& path);
    const TileStyleProfile* find(const std::string& styleId) const;

private:
    std::vector<TileStyleProfile> styles_;
};

uint64_t hashPixels(const std::vector<uint8_t>& pixels);
uint32_t resolveAutotileVariant(
    const std::string& centerGroup,
    bool northSame,
    bool eastSame,
    bool southSame,
    bool westSame);

class TileGenerator
{
public:
    static constexpr int kTileSize = 32;

    std::vector<uint8_t> generateFrame(
        const std::string& tileId,
        const std::string& category,
        const TileStyleProfile& style,
        int32_t tileX,
        int32_t tileY,
        uint32_t autotileVariant,
        int frameIndex,
        int frameCount) const;
};

} // namespace tbeq::render
