#pragma once

#include <cstdint>
#include <string>

#include <SDL.h>

#include "procedural/TileGenerator.hpp"
#include "render/SpriteRenderer.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/world/TileDefCatalog.hpp"

namespace tbeq::client
{

class TilemapRenderer
{
public:
    explicit TilemapRenderer(SpriteRenderer& sprites);

    TilemapRenderer(const TilemapRenderer&) = delete;
    TilemapRenderer& operator=(const TilemapRenderer&) = delete;

    void setStyleCatalog(const render::TileStyleProfile* style);
    void setTileDefs(const world::TileDefCatalog* tileDefs);
    void setZoneSnapshot(const net::ZoneSnapshotPayload& snapshot);

    void updateAnimation(uint64_t tickMs);
    void render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh);

    SDL_Color minimapColorAt(int32_t x, int32_t y) const;

private:
    SDL_Texture* textureForTile(int32_t x, int32_t y);
    uint32_t autotileMaskAt(int32_t x, int32_t y) const;
    int frameCountForTile(const std::string& tileId) const;

    SpriteRenderer& sprites_;
    const render::TileStyleProfile* style_ = nullptr;
    const world::TileDefCatalog* tileDefs_ = nullptr;
    net::ZoneSnapshotPayload snapshot_;
    uint64_t animTickMs_ = 0;
};

} // namespace tbeq::client
