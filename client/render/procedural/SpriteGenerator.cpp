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
    uint8_t a = 0;
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

void tintPixelBuffer(std::vector<uint8_t>& pixels, const EntityRaceTint& tint)
{
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
    {
        if (pixels[index + 3] < 8)
        {
            continue;
        }

        Rgba color{pixels[index], pixels[index + 1], pixels[index + 2], pixels[index + 3]};
        color = applyTint(color, tint);
        pixels[index] = color.r;
        pixels[index + 1] = color.g;
        pixels[index + 2] = color.b;
    }
}

void blendRegionTint(
    std::vector<uint8_t>& pixels,
    int width,
    int height,
    const std::string& hexColor,
    float mix,
    int x0,
    int y0,
    int x1,
    int y1)
{
    if (hexColor.empty() || mix <= 0.0f)
    {
        return;
    }

    const Rgba tintColor = parseHexColor(hexColor);
    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (x < 0 || y < 0 || x >= width || y >= height)
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>((y * width + x) * 4);
            if (pixels[index + 3] < 8)
            {
                continue;
            }
            Rgba color{pixels[index], pixels[index + 1], pixels[index + 2], pixels[index + 3]};
            color = lerpColor(color, tintColor, mix);
            pixels[index] = color.r;
            pixels[index + 1] = color.g;
            pixels[index + 2] = color.b;
        }
    }
}

void applyGearTintsToBuffer(std::vector<uint8_t>& pixels, int width, int height, const GearLayerTints& gearTints)
{
    blendRegionTint(pixels, width, height, gearTints.head, 0.55f, 8, 0, 23, 11);
    blendRegionTint(pixels, width, height, gearTints.chest, 0.45f, 6, 10, 25, 21);
    blendRegionTint(pixels, width, height, gearTints.hands, 0.50f, 0, 12, 8, 22);
    blendRegionTint(pixels, width, height, gearTints.hands, 0.50f, 24, 12, width - 1, 22);
    blendRegionTint(pixels, width, height, gearTints.weapon, 0.70f, 22, 8, width - 1, 24);
}

bool inRect(int x, int y, int left, int top, int right, int bottom)
{
    return x >= left && x <= right && y >= top && y <= bottom;
}

bool inEllipse(int x, int y, int cx, int cy, int rx, int ry)
{
    if (rx <= 0 || ry <= 0)
    {
        return false;
    }
    const float dx = static_cast<float>(x - cx) / static_cast<float>(rx);
    const float dy = static_cast<float>(y - cy) / static_cast<float>(ry);
    return (dx * dx + dy * dy) <= 1.0f;
}

int transformX(int x, int centerX, Facing facing)
{
    if (facing == Facing::West || facing == Facing::East)
    {
        const int mirrored = centerX + (centerX - x);
        return facing == Facing::West ? mirrored : x;
    }
    return x;
}

void setPixel(std::vector<uint8_t>& pixels, int x, int y, const Rgba& color)
{
    if (x < 0 || y < 0 || x >= SpriteGenerator::kSpriteSize || y >= SpriteGenerator::kSpriteSize || color.a == 0)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>((y * SpriteGenerator::kSpriteSize + x) * 4);
    const float alpha = color.a / 255.0f;
    if (alpha >= 0.99f)
    {
        pixels[index + 0] = color.r;
        pixels[index + 1] = color.g;
        pixels[index + 2] = color.b;
        pixels[index + 3] = color.a;
        return;
    }

    const float inv = 1.0f - alpha;
    pixels[index + 0] = static_cast<uint8_t>(color.r * alpha + pixels[index + 0] * inv);
    pixels[index + 1] = static_cast<uint8_t>(color.g * alpha + pixels[index + 1] * inv);
    pixels[index + 2] = static_cast<uint8_t>(color.b * alpha + pixels[index + 2] * inv);
    pixels[index + 3] = static_cast<uint8_t>(std::min(255.0f, color.a + pixels[index + 3] * inv));
}

bool inTriangle(int x, int y, int x0, int y0, int x1, int y1)
{
    const int x2 = x0 + (x1 - x0) * 2;
    const int y2 = y0 + (y1 - y0) * 2;
    const int d1 = (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
    const int d2 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
    const int d3 = (x - x2) * (y0 - y2) - (y - y2) * (x0 - x2);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

void drawShadow(std::vector<uint8_t>& pixels, int centerX, int feetY)
{
    for (int y = feetY; y <= feetY + 1; ++y)
    {
        for (int x = centerX - 7; x <= centerX + 7; ++x)
        {
            if (inEllipse(x, y, centerX, feetY, 7, 2))
            {
                setPixel(pixels, x, y, Rgba{0, 0, 0, 90});
            }
        }
    }
}

void drawHumanoid(
    std::vector<uint8_t>& pixels,
    int centerX,
    int feetY,
    int frameIndex,
    AnimationClipId clipId,
    Facing facing,
    const Rgba& skin,
    const Rgba& garment,
    const Rgba& highlight,
    const Rgba& outline,
    const GearLayerTints& gearTints,
    bool isNpc,
    const EntityNpcRole& npcRole,
    uint8_t entityType)
{
    const int walkPhase = frameIndex % 4;
    const int idleBob = (clipId == AnimationClipId::Idle) ? (frameIndex % 2) : 0;
    const int legSpread = (clipId == AnimationClipId::Walk)
        ? ((walkPhase == 1 || walkPhase == 3) ? 1 : 0)
        : 0;
    const int armSwing = (clipId == AnimationClipId::Walk)
        ? ((walkPhase == 0 || walkPhase == 2) ? 1 : -1)
        : 0;
    const int bobOffset = idleBob + ((clipId == AnimationClipId::Walk && walkPhase % 2 == 1) ? 1 : 0);

    const int bodyWidth = (isNpc && npcRole.silhouette == "wide") ? 10 : 8;
    const int bodyHeight = (isNpc && npcRole.silhouette == "tall") ? 14 : 12;
    const int headRadiusX = isNpc ? 5 : 4;
    const int headRadiusY = isNpc ? 4 : 3;
    const int bodyTop = feetY - bodyHeight - headRadiusY * 2 - bobOffset;
    const int headCenterY = bodyTop + headRadiusY;
    const int bodyLeft = centerX - bodyWidth / 2;
    const int bodyRight = centerX + bodyWidth / 2;
    const int bodyBottom = feetY - 2 - bobOffset;
    const int armTop = bodyTop + 3;
    const int armBottom = bodyBottom - 4;
    const int legTop = bodyBottom - 1;
    const int legWidth = isNpc ? 3 : 2;
    const int leftLegLeft = centerX - 4 - legSpread;
    const int rightLegLeft = centerX + 2 + legSpread;
    const int leftArmLeft = bodyLeft - 4;
    const int rightArmLeft = bodyRight + 1;
    const int armShift = armSwing;

    drawShadow(pixels, centerX, feetY);

    for (int y = 0; y < SpriteGenerator::kSpriteSize; ++y)
    {
        for (int x = 0; x < SpriteGenerator::kSpriteSize; ++x)
        {
            const int drawX = transformX(x, centerX, facing);
            const int drawY = y - bobOffset;
            Rgba color{};
            color.a = 0;

            const bool head = inEllipse(drawX, drawY, centerX, headCenterY, headRadiusX, headRadiusY);
            const bool body = inRect(drawX, drawY, bodyLeft, bodyTop + 1, bodyRight, bodyBottom);
            const bool leftArm = inRect(
                drawX,
                drawY,
                leftArmLeft,
                armTop + std::max(0, -armShift),
                bodyLeft - 1,
                armBottom + std::max(0, -armShift));
            const bool rightArm = inRect(
                drawX,
                drawY,
                rightArmLeft,
                armTop + std::max(0, armShift),
                bodyRight + 4,
                armBottom + std::max(0, armShift));
            const bool leftLeg = inRect(drawX, drawY, leftLegLeft, legTop, leftLegLeft + legWidth, feetY - bobOffset);
            const bool rightLeg = inRect(drawX, drawY, rightLegLeft, legTop, rightLegLeft + legWidth, feetY - bobOffset);

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
                color = lerpColor(garment, outline, 0.12f);
                color.a = 255;
            }

            if (color.a > 0)
            {
                const bool edge = head
                    && !inEllipse(drawX, drawY, centerX, headCenterY, headRadiusX + 1, headRadiusY + 1);
                if (edge || (body && (drawX == bodyLeft || drawX == bodyRight || drawY == bodyTop + 1)))
                {
                    color = lerpColor(color, outline, 0.35f);
                }
                setPixel(pixels, x, y, color);
            }
        }
    }

    if (!gearTints.weapon.empty() && entityType == 0)
    {
        const Rgba weaponColor = parseHexColor(gearTints.weapon);
        const int weaponLeft = bodyRight + 2;
        const int weaponRight = bodyRight + 5;
        const int weaponTop = armTop + armShift;
        const int weaponBottom = armBottom + armSwing + 2;
        for (int y = 0; y < SpriteGenerator::kSpriteSize; ++y)
        {
            for (int x = 0; x < SpriteGenerator::kSpriteSize; ++x)
            {
                const int drawX = transformX(x, centerX, facing);
                const int drawY = y - bobOffset;
                if (inRect(drawX, drawY, weaponLeft, weaponTop, weaponRight, weaponBottom))
                {
                    setPixel(pixels, x, y, Rgba{weaponColor.r, weaponColor.g, weaponColor.b, 255});
                }
            }
        }
    }
}

void drawQuadruped(
    std::vector<uint8_t>& pixels,
    int centerX,
    int feetY,
    int frameIndex,
    AnimationClipId clipId,
    Facing facing,
    const Rgba& bodyColor,
    const Rgba& highlight,
    const Rgba& shadow,
    float scale,
    bool small)
{
    const int walkPhase = frameIndex % 4;
    const int bobOffset = (clipId == AnimationClipId::Walk && walkPhase % 2 == 1) ? 1 : 0;
    const int legLift = (clipId == AnimationClipId::Walk)
        ? ((walkPhase == 1) ? -1 : (walkPhase == 3 ? 1 : 0))
        : 0;

    const int bodyW = static_cast<int>((small ? 10 : 14) * scale);
    const int bodyH = static_cast<int>((small ? 6 : 8) * scale);
    const int headR = static_cast<int>((small ? 3 : 4) * scale);
    const int bodyLeft = centerX - bodyW / 2;
    const int bodyRight = centerX + bodyW / 2;
    const int bodyTop = feetY - bodyH - headR * 2 - bobOffset;
    const int headOffsetX = (facing == Facing::East) ? bodyW / 3 : ((facing == Facing::West) ? -bodyW / 3 : 0);
    const int headCenterX = centerX + headOffsetX;
    const int headCenterY = bodyTop + headR;

    drawShadow(pixels, centerX, feetY);

    for (int y = 0; y < SpriteGenerator::kSpriteSize; ++y)
    {
        for (int x = 0; x < SpriteGenerator::kSpriteSize; ++x)
        {
            const int drawX = transformX(x, centerX, facing);
            const int drawY = y - bobOffset;
            Rgba color{};
            color.a = 0;

            const bool body = inRect(drawX, drawY, bodyLeft, bodyTop, bodyRight, bodyTop + bodyH);
            const bool head = inEllipse(drawX, drawY, headCenterX, headCenterY, headR + 1, headR);
            const bool tail = inRect(drawX, drawY, bodyLeft - 4, bodyTop + 2, bodyLeft - 1, bodyTop + 4);
            const bool leg1 = inRect(
                drawX,
                drawY,
                centerX - 5,
                bodyTop + bodyH - 1 + legLift,
                centerX - 3,
                feetY - bobOffset);
            const bool leg2 = inRect(
                drawX,
                drawY,
                centerX + 1,
                bodyTop + bodyH - 1 - legLift,
                centerX + 3,
                feetY - bobOffset);
            const bool leg3 = inRect(
                drawX,
                drawY,
                centerX - 2,
                bodyTop + bodyH - 1 - legLift,
                centerX,
                feetY - bobOffset);
            const bool leg4 = inRect(
                drawX,
                drawY,
                centerX + 2,
                bodyTop + bodyH - 1 + legLift,
                centerX + 4,
                feetY - bobOffset);
            const bool ear = small
                && (inTriangle(drawX, drawY, headCenterX - 2, headCenterY - headR, headCenterX, headCenterY - headR - 2)
                    || inTriangle(drawX, drawY, headCenterX + 2, headCenterY - headR, headCenterX, headCenterY - headR - 2));

            if (head || ear)
            {
                color = lerpColor(bodyColor, highlight, 0.25f);
                color.a = 255;
            }
            else if (body || tail)
            {
                color = bodyColor;
                color.a = 255;
            }
            else if (leg1 || leg2 || leg3 || leg4)
            {
                color = lerpColor(bodyColor, shadow, 0.25f);
                color.a = 255;
            }

            if (color.a > 0)
            {
                setPixel(pixels, x, y, color);
            }
        }
    }
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
    mobBodies_.clear();

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

    if (json.contains("mobBodies") && json["mobBodies"].is_object())
    {
        for (const auto& [mobId, entry] : json["mobBodies"].items())
        {
            MobBodyDef body;
            body.atlas = entry.value("atlas", std::string{});
            body.silhouette = entry.value("silhouette", "quadruped_small");
            body.bodyColor = entry.value("bodyColor", "#808080");
            body.scale = entry.value("scale", 1.0f);
            mobBodies_[mobId] = std::move(body);
        }
    }

    if (json.contains("atlasLayout") && json["atlasLayout"].is_object())
    {
        const auto& layout = json["atlasLayout"];
        atlasLayout_.frameWidth = layout.value("frameWidth", atlasLayout_.frameWidth);
        atlasLayout_.frameHeight = layout.value("frameHeight", atlasLayout_.frameHeight);
        atlasLayout_.idleStartRow = layout.value("idleStartRow", atlasLayout_.idleStartRow);
        atlasLayout_.walkStartRow = layout.value("walkStartRow", atlasLayout_.walkStartRow);
    }

    const std::filesystem::path dataRoot = path.parent_path();
    atlasLibrary_.setDataRoot(dataRoot);
    usesAtlases_ = false;

    if (json.contains("playerAtlas"))
    {
        playerAtlas_ = json.value("playerAtlas", std::string{});
        if (!playerAtlas_.empty())
        {
            usesAtlases_ = atlasLibrary_.loadAtlas("player", playerAtlas_);
        }
    }

    if (json.contains("npcAtlases") && json["npcAtlases"].is_object())
    {
        for (const auto& [roleId, atlasPath] : json["npcAtlases"].items())
        {
            if (!atlasPath.is_string())
            {
                continue;
            }
            npcAtlases_[roleId] = atlasPath.get<std::string>();
            usesAtlases_ = atlasLibrary_.loadAtlas(roleId, npcAtlases_[roleId]) || usesAtlases_;
        }
    }

    for (const auto& [mobId, body] : mobBodies_)
    {
        if (body.atlas.empty())
        {
            continue;
        }
        usesAtlases_ = atlasLibrary_.loadAtlas(mobId, body.atlas) || usesAtlases_;
    }

    if (json.contains("defaults") && json["defaults"].is_object())
    {
        defaultRaceId_ = json["defaults"].value("raceId", defaultRaceId_);
        defaultClassId_ = json["defaults"].value("classId", defaultClassId_);
        defaultNpcRole_ = json["defaults"].value("npcRole", defaultNpcRole_);
    }

    defaultMobBody_.silhouette = "quadruped_small";
    defaultMobBody_.bodyColor = "#808080";
    defaultMobBody_.scale = 1.0f;

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

MobBodyDef EntitySpriteCatalog::mobBody(const std::string& mobId) const
{
    const auto it = mobBodies_.find(mobId);
    if (it != mobBodies_.end())
    {
        return it->second;
    }
    return defaultMobBody_;
}

std::optional<std::string> EntitySpriteCatalog::atlasForEntity(
    uint8_t entityType,
    const std::string& appearanceId) const
{
    if (!usesAtlases_)
    {
        return std::nullopt;
    }

    if (entityType == 0)
    {
        return atlasLibrary_.hasAtlas("player") ? std::optional<std::string>("player") : std::nullopt;
    }

    if (entityType == 1)
    {
        const auto it = npcAtlases_.find(appearanceId);
        if (it != npcAtlases_.end() && atlasLibrary_.hasAtlas(appearanceId))
        {
            return appearanceId;
        }
        const auto fallback = npcAtlases_.find(defaultNpcRole_);
        if (fallback != npcAtlases_.end() && atlasLibrary_.hasAtlas(defaultNpcRole_))
        {
            return defaultNpcRole_;
        }
        return std::nullopt;
    }

    if (entityType == 2)
    {
        const MobBodyDef body = mobBody(appearanceId);
        if (!body.atlas.empty() && atlasLibrary_.hasAtlas(appearanceId))
        {
            return appearanceId;
        }
    }

    return std::nullopt;
}

std::vector<uint8_t> EntitySpriteCatalog::atlasFrame(
    const std::string& atlasId,
    AnimationClipId clipId,
    Facing facing,
    int frameIndex) const
{
    return atlasLibrary_.framePixels(atlasId, atlasLayout_, clipId, facing, frameIndex);
}

std::vector<uint8_t> SpriteGenerator::generateFrame(
    uint8_t entityType,
    const std::string& raceId,
    const std::string& classId,
    const std::string& appearanceId,
    const TileStyleProfile& style,
    const EntitySpriteCatalog& catalog,
    AnimationClipId clipId,
    Facing facing,
    int frameIndex,
    const GearLayerTints& gearTints) const
{
    if (const std::optional<std::string> atlasId = catalog.atlasForEntity(entityType, appearanceId))
    {
        std::vector<uint8_t> pixels = catalog.atlasFrame(*atlasId, clipId, facing, frameIndex);
        if (entityType == 0)
        {
            tintPixelBuffer(pixels, catalog.raceTint(raceId));
            applyGearTintsToBuffer(pixels, kSpriteSize, kSpriteSize, gearTints);
        }
        return pixels;
    }

    const Rgba base = parseHexColor(style.baseColor);
    const Rgba accent = parseHexColor(style.accentColor);
    const Rgba shadow = parseHexColor(style.shadowColor);
    const Rgba highlight = parseHexColor(style.highlightColor);

    std::vector<uint8_t> pixels(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 0);
    const int centerX = kSpriteSize / 2;
    const int feetY = kSpriteSize - 4;

    if (entityType == 2)
    {
        const MobBodyDef mobBody = catalog.mobBody(appearanceId);
        const Rgba bodyColor = parseHexColor(mobBody.bodyColor);
        if (mobBody.silhouette == "humanoid_small")
        {
            EntityNpcRole role;
            role.roleColor = mobBody.bodyColor;
            role.silhouette = "default";
            const Rgba skin = lerpColor(bodyColor, highlight, 0.25f);
            const Rgba garment = lerpColor(bodyColor, shadow, 0.35f);
            const Rgba outline = lerpColor(bodyColor, shadow, 0.65f);
            drawHumanoid(
                pixels,
                centerX,
                feetY,
                frameIndex,
                clipId,
                facing,
                skin,
                garment,
                highlight,
                outline,
                {},
                true,
                role,
                entityType);
        }
        else
        {
            const bool small = mobBody.silhouette.find("small") != std::string::npos;
            drawQuadruped(
                pixels,
                centerX,
                feetY,
                frameIndex,
                clipId,
                facing,
                bodyColor,
                highlight,
                shadow,
                mobBody.scale,
                small);
        }
        return pixels;
    }

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

    drawHumanoid(
        pixels,
        centerX,
        feetY,
        frameIndex,
        clipId,
        facing,
        skin,
        garment,
        highlight,
        outline,
        gearTints,
        isNpc,
        npcRole,
        entityType);

    return pixels;
}

} // namespace tbeq::render
