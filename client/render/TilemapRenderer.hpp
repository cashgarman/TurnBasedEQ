#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "procedural/TileGenerator.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/world/TileDefCatalog.hpp"

namespace tbeq::client
{

class TilemapRenderer
{
public:
    explicit TilemapRenderer(SDL_Renderer* renderer);
    ~TilemapRenderer();

    TilemapRenderer(const TilemapRenderer&) = delete;
    TilemapRenderer& operator=(const TilemapRenderer&) = delete;

    void setStyleCatalog(const render::TileStyleProfile* style);
    void setTileDefs(const world::TileDefCatalog* tileDefs);
    void setZoneSnapshot(const net::ZoneSnapshotPayload& snapshot);

    void updateAnimation(uint64_t tickMs);
    void render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh);

    SDL_Color minimapColorAt(int32_t x, int32_t y) const;

private:
    struct TextureKey
    {
        std::string tileId;
        uint32_t autotileVariant;
        int frameIndex;

        bool operator==(const TextureKey& other) const
        {
            return tileId == other.tileId
                && autotileVariant == other.autotileVariant
                && frameIndex == other.frameIndex;
        }
    };

    struct TextureKeyHash
    {
        std::size_t operator()(const TextureKey& key) const
        {
            return std::hash<std::string>{}(key.tileId)
                ^ (static_cast<std::size_t>(key.autotileVariant) << 1)
                ^ (static_cast<std::size_t>(key.frameIndex) << 2);
        }
    };

    SDL_Texture* textureForTile(int32_t x, int32_t y);
    uint32_t autotileMaskAt(int32_t x, int32_t y) const;
    int frameCountForTile(const std::string& tileId) const;

    SDL_Renderer* renderer_ = nullptr;
    const render::TileStyleProfile* style_ = nullptr;
    const world::TileDefCatalog* tileDefs_ = nullptr;
    net::ZoneSnapshotPayload snapshot_;
    render::TileGenerator generator_;
    std::unordered_map<TextureKey, SDL_Texture*, TextureKeyHash> textures_;
    uint64_t animTickMs_ = 0;
};

} // namespace tbeq::client
