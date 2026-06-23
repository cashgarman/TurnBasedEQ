#include "procedural/ItemIconGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace tbeq::render
{

namespace
{

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

bool inRect(int x, int y, int left, int top, int right, int bottom)
{
    return x >= left && x <= right && y >= top && y <= bottom;
}

void setPixel(std::vector<uint8_t>& pixels, int x, int y, const Rgba& color)
{
    if (x < 0 || y < 0 || x >= ItemIconGenerator::kIconSize || y >= ItemIconGenerator::kIconSize)
    {
        return;
    }

    const std::size_t index = static_cast<std::size_t>((y * ItemIconGenerator::kIconSize + x) * 4);
    pixels[index + 0] = color.r;
    pixels[index + 1] = color.g;
    pixels[index + 2] = color.b;
    pixels[index + 3] = color.a;
}

void fillShape(
    std::vector<uint8_t>& pixels,
    const std::string& iconShape,
    const Rgba& fill,
    const Rgba& edge)
{
    const int centerX = ItemIconGenerator::kIconSize / 2;
    const int centerY = ItemIconGenerator::kIconSize / 2;

    for (int y = 0; y < ItemIconGenerator::kIconSize; ++y)
    {
        for (int x = 0; x < ItemIconGenerator::kIconSize; ++x)
        {
            bool inside = false;

            if (iconShape == "dagger")
            {
                inside = inRect(x, y, centerX - 1, 2, centerX + 1, centerY + 4)
                    || inRect(x, y, centerX - 1, centerY + 3, centerX + 1, ItemIconGenerator::kIconSize - 3);
            }
            else if (iconShape == "sword")
            {
                inside = inRect(x, y, centerX - 1, 2, centerX + 1, centerY + 3)
                    || inRect(x, y, centerX - 2, centerY + 2, centerX + 2, centerY + 4)
                    || inRect(x, y, centerX - 1, centerY + 4, centerX + 1, ItemIconGenerator::kIconSize - 4);
            }
            else if (iconShape == "helm")
            {
                inside = inRect(x, y, centerX - 4, 4, centerX + 4, centerY + 2)
                    || inRect(x, y, centerX - 3, centerY + 1, centerX + 3, centerY + 5);
            }
            else if (iconShape == "chest")
            {
                inside = inRect(x, y, centerX - 5, 4, centerX + 5, ItemIconGenerator::kIconSize - 3);
            }
            else if (iconShape == "gloves")
            {
                inside = inRect(x, y, 2, centerY - 2, 6, ItemIconGenerator::kIconSize - 3)
                    || inRect(x, y, ItemIconGenerator::kIconSize - 7, centerY - 2, ItemIconGenerator::kIconSize - 3, ItemIconGenerator::kIconSize - 3);
            }
            else if (iconShape == "staff")
            {
                inside = inRect(x, y, centerX - 1, 2, centerX + 1, ItemIconGenerator::kIconSize - 3)
                    || inRect(x, y, centerX - 2, 2, centerX + 2, 5);
            }
            else
            {
                inside = inRect(x, y, 3, 3, ItemIconGenerator::kIconSize - 4, ItemIconGenerator::kIconSize - 4);
            }

            if (inside)
            {
                const bool border = !inRect(x, y, 4, 4, ItemIconGenerator::kIconSize - 5, ItemIconGenerator::kIconSize - 5)
                    && iconShape == "square";
                setPixel(pixels, x, y, border ? edge : fill);
            }
        }
    }
}

} // namespace

std::vector<uint8_t> ItemIconGenerator::generate(
    const std::string& iconShape,
    const std::string& tintHex) const
{
    const Rgba tint = parseHexColor(tintHex);
    const Rgba fill = tint;
    const Rgba edge = lerpColor(tint, Rgba{0, 0, 0, 255}, 0.45f);

    std::vector<uint8_t> pixels(
        static_cast<std::size_t>(kIconSize * kIconSize * 4),
        0);
    fillShape(pixels, iconShape, fill, edge);
    return pixels;
}

} // namespace tbeq::render
