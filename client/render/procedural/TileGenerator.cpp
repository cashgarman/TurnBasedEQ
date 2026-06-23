#include "procedural/TileGenerator.hpp"

#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::render
{

namespace
{

uint64_t mixSeed(uint64_t value)
{
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

uint64_t tileSeed(int64_t masterSeed, int32_t x, int32_t y, int frameIndex)
{
    uint64_t seed = static_cast<uint64_t>(masterSeed);
    seed ^= static_cast<uint64_t>(x) * 0x9E3779B97F4A7C15ULL;
    seed ^= static_cast<uint64_t>(y) * 0xBF58476D1CE4E5B9ULL;
    seed ^= static_cast<uint64_t>(frameIndex) * 0x94D049BB133111EBULL;
    return mixSeed(seed);
}

double random01(uint64_t seed)
{
    return static_cast<double>(mixSeed(seed)) / static_cast<double>(UINT64_MAX);
}

struct Rgba
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

Rgba parseHexColor(const std::string& hex)
{
    if (hex.size() < 7 || hex[0] != '#')
    {
        return Rgba{128, 128, 128, 255};
    }

    const auto parseByte = [&hex](std::size_t offset) -> uint8_t
    {
        const std::string byte = hex.substr(offset, 2);
        return static_cast<uint8_t>(std::stoul(byte, nullptr, 16));
    };

    return Rgba{parseByte(1), parseByte(3), parseByte(5), 255};
}

Rgba lerpColor(const Rgba& a, const Rgba& b, float t)
{
    return Rgba{
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t),
        255};
}

bool isEdgeTile(uint32_t autotileVariant)
{
    return autotileVariant != 15;
}

} // namespace

bool TileStyleCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_array())
    {
        return false;
    }

    styles_.clear();
    for (const auto& entry : json)
    {
        TileStyleProfile profile;
        profile.id = entry.value("id", "");
        profile.masterSeed = entry.value("masterSeed", 0);
        if (entry.contains("palette"))
        {
            profile.baseColor = entry["palette"].value("base", "#808080");
            profile.accentColor = entry["palette"].value("accent", "#909090");
            profile.shadowColor = entry["palette"].value("shadow", "#404040");
            profile.highlightColor = entry["palette"].value("highlight", "#c0c0c0");
        }
        profile.noiseScale = entry.value("noiseScale", 0.15f);
        profile.hueShift = entry.value("hueShift", 0.0f);
        profile.saturation = entry.value("saturation", 1.0f);
        styles_.push_back(std::move(profile));
    }

    return !styles_.empty();
}

const TileStyleProfile* TileStyleCatalog::find(const std::string& styleId) const
{
    for (const auto& style : styles_)
    {
        if (style.id == styleId)
        {
            return &style;
        }
    }
    return styles_.empty() ? nullptr : &styles_.front();
}

uint64_t hashPixels(const std::vector<uint8_t>& pixels)
{
    uint64_t hash = 1469598103934665603ULL;
    for (uint8_t value : pixels)
    {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint32_t resolveAutotileVariant(
    const std::string& centerGroup,
    bool northSame,
    bool eastSame,
    bool southSame,
    bool westSame)
{
    (void)centerGroup;
    uint32_t mask = 0;
    if (northSame)
    {
        mask |= 1U;
    }
    if (eastSame)
    {
        mask |= 2U;
    }
    if (southSame)
    {
        mask |= 4U;
    }
    if (westSame)
    {
        mask |= 8U;
    }
    return mask;
}

std::vector<uint8_t> TileGenerator::generateFrame(
    const std::string& tileId,
    const std::string& category,
    const TileStyleProfile& style,
    int32_t tileX,
    int32_t tileY,
    uint32_t autotileVariant,
    int frameIndex,
    int frameCount) const
{
    const Rgba base = parseHexColor(style.baseColor);
    const Rgba accent = parseHexColor(style.accentColor);
    const Rgba shadow = parseHexColor(style.shadowColor);
    const Rgba highlight = parseHexColor(style.highlightColor);

    std::vector<uint8_t> pixels(static_cast<std::size_t>(kTileSize * kTileSize * 4));

    for (int y = 0; y < kTileSize; ++y)
    {
        for (int x = 0; x < kTileSize; ++x)
        {
            const uint64_t seed = tileSeed(
                style.masterSeed + static_cast<int64_t>(autotileVariant),
                tileX + x,
                tileY + y,
                frameIndex);
            const double noise = random01(seed);

            Rgba color = lerpColor(base, accent, static_cast<float>(noise * style.noiseScale));

            if (category == "water")
            {
                const double wave = std::sin((x + y + frameIndex) * 0.35);
                color = lerpColor(shadow, highlight, static_cast<float>((wave + 1.0) * 0.35 + noise * 0.2));
                if (isEdgeTile(autotileVariant) && (x == 0 || y == 0 || x == kTileSize - 1 || y == kTileSize - 1))
                {
                    color = lerpColor(color, highlight, 0.45f);
                }
            }
            else if (category == "portal")
            {
                const double ring = std::abs(std::sin((x - 16) * 0.25 + (y - 16) * 0.25 + frameIndex * 0.5));
                color = lerpColor(accent, highlight, static_cast<float>(ring));
                if (((x + y + frameIndex) % 5) == 0)
                {
                    color = lerpColor(color, highlight, 0.6f);
                }
            }
            else if (category == "grass" && frameCount > 1)
            {
                const int sway = static_cast<int>(std::sin(frameIndex * 0.8 + x * 0.2) * 1.5);
                color = lerpColor(color, highlight, static_cast<float>((x + sway) % 7 == 0 ? 0.25 : 0.0));
                if (tileId == "grass_flowers" && (x + y + frameIndex) % 11 == 0)
                {
                    color = lerpColor(color, accent, 0.55f);
                }
                if (tileId == "bush" && frameCount > 1)
                {
                    const double rustle = std::sin((x + frameIndex) * 0.4);
                    color = lerpColor(color, shadow, static_cast<float>(rustle > 0.6 ? 0.15 : 0.0));
                }
                if (tileId == "tree_canopy")
                {
                    color = lerpColor(shadow, accent, static_cast<float>(0.35 + noise * 0.25));
                }
            }
            else if (category == "lava" && frameCount > 1)
            {
                const double bubble = std::sin((x * 0.5 + y * 0.3 + frameIndex) * 0.7);
                color = lerpColor(shadow, highlight, static_cast<float>((bubble + 1.0) * 0.4));
                if ((x + y + frameIndex) % 9 == 0)
                {
                    color = lerpColor(color, highlight, 0.7f);
                }
            }
            else if (category == "stone" || category == "cobble")
            {
                if ((x % 4 == 0 || y % 4 == 0) && noise > 0.55)
                {
                    color = lerpColor(color, shadow, 0.25f);
                }
                if (tileId == "stone_wall" && frameCount > 1)
                {
                    const double flicker = std::sin(frameIndex * 0.9 + x * 0.1);
                    color = lerpColor(color, highlight, static_cast<float>(flicker > 0.7 ? 0.12 : 0.0));
                }
            }
            else if (category == "wood")
            {
                if (y % 5 == 0)
                {
                    color = lerpColor(color, shadow, 0.2f);
                }
                if ((x + y) % 13 == 0)
                {
                    color = lerpColor(color, accent, 0.35f);
                }
            }
            else if (category == "ice" || category == "snow")
            {
                if (frameCount > 1 && (x + y + frameIndex) % 8 == 0)
                {
                    color = lerpColor(color, highlight, 0.55f);
                }
            }
            else if (category == "swamp" && frameCount > 1)
            {
                const double ripple = std::sin((x + y + frameIndex) * 0.25);
                color = lerpColor(color, accent, static_cast<float>((ripple + 1.0) * 0.15));
            }

            if (isEdgeTile(autotileVariant) && (x == 0 || y == 0 || x == kTileSize - 1 || y == kTileSize - 1))
            {
                color = lerpColor(color, shadow, 0.35f);
            }

            const std::size_t index = static_cast<std::size_t>((y * kTileSize + x) * 4);
            pixels[index + 0] = color.r;
            pixels[index + 1] = color.g;
            pixels[index + 2] = color.b;
            pixels[index + 3] = color.a;
        }
    }

    return pixels;
}

} // namespace tbeq::render
