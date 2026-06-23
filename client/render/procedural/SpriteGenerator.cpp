#include "procedural/SpriteGenerator.hpp"

#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

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

void rgbToHsl(float r, float g, float b, float& h, float& s, float& l)
{
    const float maxC = std::max({r, g, b});
    const float minC = std::min({r, g, b});
    l = (maxC + minC) * 0.5f;
    if (maxC == minC)
    {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    const float delta = maxC - minC;
    s = l > 0.5f ? delta / (2.0f - maxC - minC) : delta / (maxC + minC);
    if (maxC == r)
    {
        h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
    }
    else if (maxC == g)
    {
        h = (b - r) / delta + 2.0f;
    }
    else
    {
        h = (r - g) / delta + 4.0f;
    }
    h /= 6.0f;
}

Rgba hslToRgb(float h, float s, float l)
{
    auto hueToRgb = [](float p, float q, float t)
    {
        if (t < 0.0f)
        {
            t += 1.0f;
        }
        if (t > 1.0f)
        {
            t -= 1.0f;
        }
        if (t < 1.0f / 6.0f)
        {
            return p + (q - p) * 6.0f * t;
        }
        if (t < 1.0f / 2.0f)
        {
            return q;
        }
        if (t < 2.0f / 3.0f)
        {
            return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        }
        return p;
    };

    if (s == 0.0f)
    {
        const uint8_t gray = static_cast<uint8_t>(l * 255.0f);
        return Rgba{gray, gray, gray, 255};
    }

    const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    const float p = 2.0f * l - q;
    return Rgba{
        static_cast<uint8_t>(hueToRgb(p, q, h + 1.0f / 3.0f) * 255.0f),
        static_cast<uint8_t>(hueToRgb(p, q, h) * 255.0f),
        static_cast<uint8_t>(hueToRgb(p, q, h - 1.0f / 3.0f) * 255.0f),
        255};
}

Rgba applyTint(const Rgba& color, const EntityRaceTint& tint)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, h, s, l);
    h += tint.hueOffset / 360.0f;
    if (h > 1.0f)
    {
        h -= 1.0f;
    }
    if (h < 0.0f)
    {
        h += 1.0f;
    }
    s = std::clamp(s * tint.saturation, 0.0f, 1.0f);
    l = std::clamp(l + tint.lightness, 0.05f, 0.95f);
    return hslToRgb(h, s, l);
}

bool inRect(int x, int y, int left, int top, int right, int bottom)
{
    return x >= left && x <= right && y >= top && y <= bottom;
}

bool inEllipse(int x, int y, int cx, int cy, int rx, int ry)
{
    const float dx = static_cast<float>(x - cx) / static_cast<float>(rx);
    const float dy = static_cast<float>(y - cy) / static_cast<float>(ry);
    return (dx * dx + dy * dy) <= 1.0f;
}

} // namespace

bool EntitySpriteCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_object())
    {
        return false;
    }

    races_.clear();
    classes_.clear();
    npcRoles_.clear();

    if (json.contains("races") && json["races"].is_object())
    {
        for (const auto& [raceId, entry] : json["races"].items())
        {
            EntityRaceTint tint;
            tint.hueOffset = entry.value("hueOffset", 0.0f);
            tint.saturation = entry.value("saturation", 1.0f);
            tint.lightness = entry.value("lightness", 0.0f);
            races_[raceId] = tint;
        }
    }

    if (json.contains("classes") && json["classes"].is_object())
    {
        for (const auto& [classId, entry] : json["classes"].items())
        {
            EntityClassTint tint;
            tint.accentMix = entry.value("accentMix", 0.3f);
            tint.shadowMix = entry.value("shadowMix", 0.5f);
            classes_[classId] = tint;
        }
    }

    if (json.contains("npcRoles") && json["npcRoles"].is_object())
    {
        for (const auto& [roleId, entry] : json["npcRoles"].items())
        {
            EntityNpcRole role;
            role.roleColor = entry.value("roleColor", "#808080");
            role.silhouette = entry.value("silhouette", "default");
            npcRoles_[roleId] = std::move(role);
        }
    }

    if (json.contains("defaults") && json["defaults"].is_object())
    {
        defaultRaceId_ = json["defaults"].value("raceId", defaultRaceId_);
        defaultClassId_ = json["defaults"].value("classId", defaultClassId_);
        defaultNpcRole_ = json["defaults"].value("npcRole", defaultNpcRole_);
    }

    return !races_.empty() && !classes_.empty() && !npcRoles_.empty();
}

EntityRaceTint EntitySpriteCatalog::raceTint(const std::string& raceId) const
{
    const auto it = races_.find(raceId);
    if (it != races_.end())
    {
        return it->second;
    }
    const auto fallback = races_.find(defaultRaceId_);
    return fallback != races_.end() ? fallback->second : EntityRaceTint{};
}

EntityClassTint EntitySpriteCatalog::classTint(const std::string& classId) const
{
    const auto it = classes_.find(classId);
    if (it != classes_.end())
    {
        return it->second;
    }
    const auto fallback = classes_.find(defaultClassId_);
    return fallback != classes_.end() ? fallback->second : EntityClassTint{};
}

EntityNpcRole EntitySpriteCatalog::npcRole(const std::string& appearanceId) const
{
    const auto it = npcRoles_.find(appearanceId);
    if (it != npcRoles_.end())
    {
        return it->second;
    }
    const auto fallback = npcRoles_.find(defaultNpcRole_);
    return fallback != npcRoles_.end() ? fallback->second : EntityNpcRole{};
}

std::vector<uint8_t> SpriteGenerator::generateFrame(
    uint8_t entityType,
    const std::string& raceId,
    const std::string& classId,
    const std::string& appearanceId,
    const TileStyleProfile& style,
    const EntitySpriteCatalog& catalog,
    int frameIndex,
    const GearLayerTints& gearTints) const
{
    const Rgba base = parseHexColor(style.baseColor);
    const Rgba accent = parseHexColor(style.accentColor);
    const Rgba shadow = parseHexColor(style.shadowColor);
    const Rgba highlight = parseHexColor(style.highlightColor);

    const int bobOffset = (frameIndex % 2 == 0) ? 0 : 1;
    const bool isNpc = entityType != 0;

    EntityRaceTint raceTint{};
    EntityClassTint classTint{};
    EntityNpcRole npcRole{};
    Rgba skin = lerpColor(base, highlight, 0.45f);
    Rgba garment = lerpColor(base, accent, 0.35f);
    Rgba outline = shadow;

    if (isNpc)
    {
        npcRole = catalog.npcRole(appearanceId);
        const Rgba roleColor = parseHexColor(npcRole.roleColor);
        skin = lerpColor(roleColor, highlight, 0.25f);
        garment = lerpColor(roleColor, shadow, 0.35f);
        outline = lerpColor(roleColor, shadow, 0.65f);
    }
    else
    {
        raceTint = catalog.raceTint(raceId);
        classTint = catalog.classTint(classId);
        skin = applyTint(lerpColor(base, highlight, 0.5f), raceTint);
        garment = applyTint(lerpColor(base, accent, classTint.accentMix), raceTint);
        outline = applyTint(lerpColor(shadow, accent, classTint.shadowMix), raceTint);
    }

    const int bodyWidth = (isNpc && npcRole.silhouette == "wide") ? 10 : 8;
    const int bodyHeight = (isNpc && npcRole.silhouette == "tall") ? 14 : 12;
    const int headRadiusX = isNpc ? 5 : 4;
    const int headRadiusY = isNpc ? 4 : 3;
    const int centerX = kSpriteSize / 2;
    const int feetY = kSpriteSize - 4 + bobOffset;
    const int bodyTop = feetY - bodyHeight - headRadiusY * 2;
    const int headCenterY = bodyTop + headRadiusY;
    const int bodyLeft = centerX - bodyWidth / 2;
    const int bodyRight = centerX + bodyWidth / 2;
    const int bodyBottom = feetY - 2;
    const int armTop = bodyTop + 3;
    const int armBottom = bodyBottom - 4;
    const int legTop = bodyBottom - 1;
    const int legWidth = isNpc ? 3 : 2;
    const int leftLegLeft = centerX - 4;
    const int rightLegLeft = centerX + 2;

    std::vector<uint8_t> pixels(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 0);

    for (int y = 0; y < kSpriteSize; ++y)
    {
        for (int x = 0; x < kSpriteSize; ++x)
        {
            Rgba color{};
            color.a = 0;

            const int drawY = y - bobOffset;
            const bool head = inEllipse(x, drawY, centerX, headCenterY, headRadiusX, headRadiusY);
            const bool body = inRect(x, drawY, bodyLeft, bodyTop + 1, bodyRight, bodyBottom);
            const bool leftArm = inRect(x, drawY, bodyLeft - 4, armTop, bodyLeft - 1, armBottom);
            const bool rightArm = inRect(x, drawY, bodyRight + 1, armTop, bodyRight + 4, armBottom);
            const bool leftLeg = inRect(x, drawY, leftLegLeft, legTop, leftLegLeft + legWidth, feetY);
            const bool rightLeg = inRect(x, drawY, rightLegLeft, legTop, rightLegLeft + legWidth, feetY);
            const bool feetShadow = inRect(x, drawY, centerX - 6, feetY, centerX + 6, feetY + 1);

            if (head)
            {
                color = skin;
                if (!gearTints.head.empty())
                {
                    color = lerpColor(skin, parseHexColor(gearTints.head), 0.72f);
                }
                color.a = 255;
            }
            else if (body)
            {
                color = garment;
                if (!gearTints.chest.empty())
                {
                    color = lerpColor(garment, parseHexColor(gearTints.chest), 0.65f);
                }
                color.a = 255;
            }
            else if (leftArm || rightArm)
            {
                color = lerpColor(garment, highlight, 0.15f);
                if (!gearTints.hands.empty())
                {
                    color = lerpColor(color, parseHexColor(gearTints.hands), 0.70f);
                }
                color.a = 255;
            }
            else if (leftLeg || rightLeg)
            {
                color = garment;
                color.a = 255;
            }
            else if (feetShadow)
            {
                color = lerpColor(outline, shadow, 0.5f);
                color.a = 180;
            }

            if (color.a > 0)
            {
                const bool edge = !inEllipse(x, drawY, centerX, headCenterY, headRadiusX + 1, headRadiusY + 1)
                    && head;
                if (edge || (body && (x == bodyLeft || x == bodyRight || drawY == bodyTop + 1)))
                {
                    color = lerpColor(color, outline, 0.35f);
                }
            }

            const std::size_t index = static_cast<std::size_t>((y * kSpriteSize + x) * 4);
            pixels[index + 0] = color.r;
            pixels[index + 1] = color.g;
            pixels[index + 2] = color.b;
            pixels[index + 3] = color.a;
        }
    }

    if (!gearTints.weapon.empty() && entityType == 0)
    {
        const Rgba weaponColor = parseHexColor(gearTints.weapon);
        const int weaponLeft = bodyRight + 2;
        const int weaponRight = bodyRight + 5;
        const int weaponTop = armTop;
        const int weaponBottom = armBottom + 2;
        for (int y = 0; y < kSpriteSize; ++y)
        {
            for (int x = 0; x < kSpriteSize; ++x)
            {
                const int drawY = y - bobOffset;
                if (!inRect(x, drawY, weaponLeft, weaponTop, weaponRight, weaponBottom))
                {
                    continue;
                }

                const std::size_t index = static_cast<std::size_t>((y * kSpriteSize + x) * 4);
                pixels[index + 0] = weaponColor.r;
                pixels[index + 1] = weaponColor.g;
                pixels[index + 2] = weaponColor.b;
                pixels[index + 3] = 255;
            }
        }
    }

    return pixels;
}

} // namespace tbeq::render
