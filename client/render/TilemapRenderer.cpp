#include "render/TilemapRenderer.hpp"

namespace tbeq::client
{

namespace
{

constexpr int kAnimFrameMs = 125;

} // namespace

TilemapRenderer::TilemapRenderer(SpriteRenderer& sprites)
    : sprites_(sprites)
{
}

void TilemapRenderer::setStyleCatalog(const render::TileStyleProfile* style)
{
    style_ = style;
    sprites_.setStyle(style);
    sprites_.setTileDefs(tileDefs_);
}

void TilemapRenderer::setTileDefs(const world::TileDefCatalog* tileDefs)
{
    tileDefs_ = tileDefs;
    sprites_.setTileDefs(tileDefs);
}

void TilemapRenderer::setZoneSnapshot(const net::ZoneSnapshotPayload& snapshot)
{
    snapshot_ = snapshot;
}

void TilemapRenderer::updateAnimation(uint64_t tickMs)
{
    animTickMs_ = tickMs;
}

int TilemapRenderer::frameCountForTile(const std::string& tileId) const
{
    const auto* def = tileDefs_ != nullptr ? tileDefs_->find(tileId) : nullptr;
    if (def == nullptr || def->animation == "none")
    {
        return 1;
    }
    if (def->frameCount > 0)
    {
        return def->frameCount;
    }
    if (def->category == "water" || def->id == "portal_pad")
    {
        return 8;
    }
    if (def->animation == "cycle")
    {
        return 6;
    }
    return 1;
}

uint32_t TilemapRenderer::autotileMaskAt(int32_t x, int32_t y) const
{
    if (tileDefs_ == nullptr || x < 0 || y < 0 || x >= snapshot_.width || y >= snapshot_.height)
    {
        return 0;
    }

    const auto centerIndex = static_cast<std::size_t>(y * snapshot_.width + x);
    const std::string centerTile = snapshot_.tiles[centerIndex];
    const std::string centerGroup = tileDefs_->autotileGroup(centerTile);

    auto sameGroup = [&](int32_t nx, int32_t ny)
    {
        if (nx < 0 || ny < 0 || nx >= snapshot_.width || ny >= snapshot_.height)
        {
            return false;
        }
        const auto index = static_cast<std::size_t>(ny * snapshot_.width + nx);
        return tileDefs_->autotileGroup(snapshot_.tiles[index]) == centerGroup;
    };

    return render::resolveAutotileVariant(
        centerGroup,
        sameGroup(x, y - 1),
        sameGroup(x + 1, y),
        sameGroup(x, y + 1),
        sameGroup(x - 1, y));
}

SDL_Texture* TilemapRenderer::textureForTile(int32_t x, int32_t y)
{
    if (style_ == nullptr || tileDefs_ == nullptr || x < 0 || y < 0 || x >= snapshot_.width || y >= snapshot_.height)
    {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(y * snapshot_.width + x);
    const std::string& tileId = snapshot_.tiles[index];
    const auto* def = tileDefs_->find(tileId);
    const std::string category = def != nullptr ? def->category : tileId;
    const uint32_t autotileVariant = autotileMaskAt(x, y);
    const int frameCount = frameCountForTile(tileId);
    const int frameIndex = frameCount > 1
        ? static_cast<int>((animTickMs_ / kAnimFrameMs + static_cast<uint64_t>(x + y)) % frameCount)
        : 0;

    return sprites_.textureForTile(tileId, category, x, y, autotileVariant, frameIndex, frameCount);
}

void TilemapRenderer::render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh)
{
    const int tileSize = render::TileGenerator::kTileSize;
    for (int viewY = 0; viewY < viewTilesHigh; ++viewY)
    {
        for (int viewX = 0; viewX < viewTilesWide; ++viewX)
        {
            const int32_t worldX = cameraTileX + viewX;
            const int32_t worldY = cameraTileY + viewY;
            SDL_Texture* texture = textureForTile(worldX, worldY);
            if (texture == nullptr)
            {
                continue;
            }

            SDL_Rect dest{
                viewX * tileSize,
                viewY * tileSize,
                tileSize,
                tileSize};
            SDL_RenderCopy(sprites_.cache().renderer(), texture, nullptr, &dest);
        }
    }
}

SDL_Color TilemapRenderer::minimapColorAt(int32_t x, int32_t y) const
{
    if (x < 0 || y < 0 || x >= snapshot_.width || y >= snapshot_.height || style_ == nullptr)
    {
        return SDL_Color{32, 32, 32, 255};
    }

    const auto index = static_cast<std::size_t>(y * snapshot_.width + x);
    const std::string& tileId = snapshot_.tiles[index];
    if (tileId.find("water") != std::string::npos)
    {
        return SDL_Color{40, 80, 160, 255};
    }
    if (tileId.find("wall") != std::string::npos)
    {
        return SDL_Color{70, 70, 70, 255};
    }
    if (tileId.find("portal") != std::string::npos)
    {
        return SDL_Color{180, 120, 255, 255};
    }
    return SDL_Color{80, 120, 60, 255};
}

} // namespace tbeq::client
