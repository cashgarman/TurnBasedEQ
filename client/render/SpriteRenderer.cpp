#include "render/SpriteRenderer.hpp"

#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::client
{

SpriteRenderer::SpriteRenderer(TextureCache& cache)
    : cache_(cache)
{
}

void SpriteRenderer::setStyle(const render::TileStyleProfile* style)
{
    style_ = style;
}

void SpriteRenderer::setSpriteCatalog(const render::EntitySpriteCatalog* catalog)
{
    spriteCatalog_ = catalog;
}

void SpriteRenderer::setItemCatalog(const content::ItemCatalog* catalog)
{
    itemCatalog_ = catalog;
}

void SpriteRenderer::setMobCatalog(const content::MobCatalog* mobCatalog)
{
    mobCatalog_ = mobCatalog;
}

void SpriteRenderer::setTileDefs(const world::TileDefCatalog* tileDefs)
{
    tileDefs_ = tileDefs;
}

uint64_t SpriteRenderer::entityTextureKey(const EntitySpriteRequest& request)
{
    uint64_t key = 1469598103934665603ULL;
    key = hashCombine(key, request.entityType);
    key = hashCombine(key, std::hash<std::string>{}(request.raceId));
    key = hashCombine(key, std::hash<std::string>{}(request.classId));
    key = hashCombine(key, std::hash<std::string>{}(request.appearanceId));
    key = hashCombine(key, static_cast<uint64_t>(request.clipId));
    key = hashCombine(key, static_cast<uint64_t>(request.facing));
    key = hashCombine(key, static_cast<uint64_t>(request.frameIndex));
    key = hashCombine(key, std::hash<std::string>{}(request.equippedWeaponItemId));
    key = hashCombine(key, std::hash<std::string>{}(request.equippedHeadItemId));
    key = hashCombine(key, std::hash<std::string>{}(request.equippedChestItemId));
    key = hashCombine(key, std::hash<std::string>{}(request.equippedHandsItemId));
    return key;
}

uint64_t SpriteRenderer::tileTextureKey(
    const std::string& tileId,
    uint32_t autotileVariant,
    int frameIndex)
{
    uint64_t key = 1099511628211ULL;
    key = hashCombine(key, std::hash<std::string>{}(tileId));
    key = hashCombine(key, autotileVariant);
    key = hashCombine(key, static_cast<uint64_t>(frameIndex));
    return key;
}

SDL_Texture* SpriteRenderer::textureForEntity(const EntitySpriteRequest& request)
{
    if (style_ == nullptr || spriteCatalog_ == nullptr)
    {
        return nullptr;
    }

    const uint64_t key = entityTextureKey(request);
    return cache_.getOrCreate(key, [this, request]()
    {
        render::GearLayerTints gearTints;
        if (itemCatalog_ != nullptr && request.entityType == 0)
        {
            const auto tintForItem = [this](const std::string& itemId) -> std::string
            {
                if (itemId.empty())
                {
                    return {};
                }
                const content::ItemDef* item = itemCatalog_->findItem(itemId);
                return (item != nullptr && !item->spriteTint.empty()) ? item->spriteTint : std::string{};
            };

            gearTints.weapon = tintForItem(request.equippedWeaponItemId);
            gearTints.head = tintForItem(request.equippedHeadItemId);
            gearTints.chest = tintForItem(request.equippedChestItemId);
            gearTints.hands = tintForItem(request.equippedHandsItemId);
        }

        const auto pixels = spriteGenerator_.generateFrame(
            request.entityType,
            request.raceId,
            request.classId,
            request.appearanceId,
            *style_,
            *spriteCatalog_,
            request.clipId,
            request.facing,
            request.frameIndex,
            gearTints);

        return cache_.createFromRgbaPixels(
            pixels.data(),
            render::SpriteGenerator::kSpriteSize,
            render::SpriteGenerator::kSpriteSize);
    });
}

SDL_Texture* SpriteRenderer::textureForTile(
    const std::string& tileId,
    const std::string& category,
    int32_t tileX,
    int32_t tileY,
    uint32_t autotileVariant,
    int frameIndex,
    int frameCount)
{
    if (style_ == nullptr)
    {
        return nullptr;
    }

    const uint64_t key = tileTextureKey(tileId, autotileVariant, frameIndex);
    return cache_.getOrCreate(key, [this, tileId, category, tileX, tileY, autotileVariant, frameIndex, frameCount]()
    {
        const auto pixels = tileGenerator_.generateFrame(
            tileId,
            category,
            *style_,
            tileX,
            tileY,
            autotileVariant,
            frameIndex,
            frameCount);

        return cache_.createFromRgbaPixels(
            pixels.data(),
            render::TileGenerator::kTileSize,
            render::TileGenerator::kTileSize);
    });
}

void SpriteRenderer::warmZone(
    const net::ZoneSnapshotPayload& snapshot,
    const std::vector<std::string>& mobIds)
{
    if (style_ == nullptr || tileDefs_ == nullptr)
    {
        return;
    }

    std::unordered_map<std::string, int> tileFrameCounts;
    for (const std::string& tileId : snapshot.tiles)
    {
        const auto* def = tileDefs_->find(tileId);
        if (def == nullptr)
        {
            continue;
        }

        int frameCount = def->frameCount;
        if (frameCount <= 0)
        {
            if (def->animation == "none")
            {
                frameCount = 1;
            }
            else if (def->category == "water" || def->id == "portal_pad")
            {
                frameCount = 8;
            }
            else
            {
                frameCount = 6;
            }
        }
        tileFrameCounts[tileId] = frameCount;
    }

    for (const auto& [tileId, frameCount] : tileFrameCounts)
    {
        const auto* def = tileDefs_->find(tileId);
        const std::string category = def != nullptr ? def->category : tileId;
        for (uint32_t variant = 0; variant < 16; ++variant)
        {
            for (int frame = 0; frame < frameCount; ++frame)
            {
                textureForTile(tileId, category, 0, 0, variant, frame, frameCount);
            }
        }
    }

    const std::vector<std::string> races = {"human", "wood_elf", "dwarf"};
    const std::vector<std::string> classes = {"warrior", "cleric", "wizard", "rogue"};
    const std::vector<std::string> npcRoles = {"merchant", "lore", "quest"};

    for (const std::string& raceId : races)
    {
        for (const std::string& classId : classes)
        {
            for (int facing = 0; facing < 4; ++facing)
            {
                for (int frame = 0; frame < 4; ++frame)
                {
                    EntitySpriteRequest request;
                    request.entityType = 0;
                    request.raceId = raceId;
                    request.classId = classId;
                    request.clipId = render::AnimationClipId::Idle;
                    request.facing = static_cast<render::Facing>(facing);
                    request.frameIndex = frame;
                    textureForEntity(request);
                }
            }
        }
    }

    for (const std::string& role : npcRoles)
    {
        for (int facing = 0; facing < 4; ++facing)
        {
            for (int frame = 0; frame < 4; ++frame)
            {
                EntitySpriteRequest request;
                request.entityType = 1;
                request.appearanceId = role;
                request.clipId = render::AnimationClipId::Idle;
                request.facing = static_cast<render::Facing>(facing);
                request.frameIndex = frame;
                textureForEntity(request);
            }
        }
    }

    for (const std::string& mobId : mobIds)
    {
        for (int facing = 0; facing < 4; ++facing)
        {
            for (int clip = 0; clip < 2; ++clip)
            {
                for (int frame = 0; frame < 4; ++frame)
                {
                    EntitySpriteRequest request;
                    request.entityType = 2;
                    request.appearanceId = mobId;
                    request.clipId = clip == 0
                        ? render::AnimationClipId::Idle
                        : render::AnimationClipId::Walk;
                    request.facing = static_cast<render::Facing>(facing);
                    request.frameIndex = frame;
                    textureForEntity(request);
                }
            }
        }
    }
}

} // namespace tbeq::client
