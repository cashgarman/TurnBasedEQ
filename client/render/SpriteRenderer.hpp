#pragma once

#include <cstdint>
#include <string>

#include <SDL.h>

#include "tbeq/net/ClientPackets.hpp"
#include "procedural/SpriteGenerator.hpp"
#include "procedural/TileGenerator.hpp"
#include "render/AnimationTypes.hpp"
#include "render/TextureCache.hpp"
#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/world/TileDefCatalog.hpp"

namespace tbeq::client
{

struct EntitySpriteRequest
{
    uint8_t entityType = 0;
    std::string raceId;
    std::string classId;
    std::string appearanceId;
    render::AnimationClipId clipId = render::AnimationClipId::Idle;
    render::Facing facing = render::Facing::South;
    int frameIndex = 0;
    std::string equippedWeaponItemId;
    std::string equippedHeadItemId;
    std::string equippedChestItemId;
    std::string equippedHandsItemId;
};

class SpriteRenderer
{
public:
    explicit SpriteRenderer(TextureCache& cache);

    void setStyle(const render::TileStyleProfile* style);
    void setSpriteCatalog(const render::EntitySpriteCatalog* catalog);
    void setItemCatalog(const content::ItemCatalog* catalog);
    void setMobCatalog(const content::MobCatalog* mobCatalog);
    void setTileDefs(const world::TileDefCatalog* tileDefs);

    SDL_Texture* textureForEntity(const EntitySpriteRequest& request);
    SDL_Texture* textureForTile(
        const std::string& tileId,
        const std::string& category,
        int32_t tileX,
        int32_t tileY,
        uint32_t autotileVariant,
        int frameIndex,
        int frameCount);

    void warmZone(
        const net::ZoneSnapshotPayload& snapshot,
        const std::vector<std::string>& mobIds);

    TextureCache& cache() { return cache_; }

    static uint64_t entityTextureKey(const EntitySpriteRequest& request);
    static uint64_t tileTextureKey(
        const std::string& tileId,
        uint32_t autotileVariant,
        int frameIndex);

private:
    TextureCache& cache_;
    const render::TileStyleProfile* style_ = nullptr;
    const render::EntitySpriteCatalog* spriteCatalog_ = nullptr;
    const content::ItemCatalog* itemCatalog_ = nullptr;
    const content::MobCatalog* mobCatalog_ = nullptr;
    const world::TileDefCatalog* tileDefs_ = nullptr;
    render::SpriteGenerator spriteGenerator_;
    render::TileGenerator tileGenerator_;
};

} // namespace tbeq::client
