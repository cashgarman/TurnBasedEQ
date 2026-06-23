#include "render/EntityRenderer.hpp"

#include <algorithm>

namespace tbeq::client
{

EntityRenderer::EntityRenderer(SDL_Renderer* renderer)
    : renderer_(renderer)
{
}

EntityRenderer::~EntityRenderer()
{
    clear();
}

void EntityRenderer::setStyleCatalog(const render::TileStyleProfile* style)
{
    style_ = style;
}

void EntityRenderer::setSpriteCatalog(const render::EntitySpriteCatalog* catalog)
{
    catalog_ = catalog;
}

void EntityRenderer::clear()
{
    for (auto& entry : textures_)
    {
        SDL_DestroyTexture(entry.second);
    }
    textures_.clear();
    entities_.clear();
    hasLocalPlayer_ = false;
}

void EntityRenderer::upsertEntity(EntityVisual entity)
{
    const auto it = entities_.find(entity.entityId);
    if (it != entities_.end())
    {
        entity.prevTileX = it->second.tileX;
        entity.prevTileY = it->second.tileY;
    }
    else
    {
        entity.prevTileX = entity.tileX;
        entity.prevTileY = entity.tileY;
    }
    entities_[entity.entityId] = std::move(entity);
}

void EntityRenderer::setLocalPlayer(
    uint32_t entityId,
    const std::string& name,
    const std::string& raceId,
    const std::string& classId,
    int32_t tileX,
    int32_t tileY)
{
    localPlayer_.entityId = entityId;
    localPlayer_.name = name;
    localPlayer_.raceId = raceId;
    localPlayer_.classId = classId;
    localPlayer_.appearanceId.clear();
    localPlayer_.entityType = 0;
    localPlayer_.tileX = tileX;
    localPlayer_.tileY = tileY;
    localPlayer_.prevTileX = tileX;
    localPlayer_.prevTileY = tileY;
    localPlayer_.isLocalPlayer = true;
    hasLocalPlayer_ = entityId != 0;
    mergeLocalPlayer();
}

void EntityRenderer::updateLocalPlayerPosition(int32_t tileX, int32_t tileY)
{
    if (!hasLocalPlayer_)
    {
        return;
    }

    localPlayer_.prevTileX = localPlayer_.tileX;
    localPlayer_.prevTileY = localPlayer_.tileY;
    localPlayer_.tileX = tileX;
    localPlayer_.tileY = tileY;
    mergeLocalPlayer();
}

void EntityRenderer::mergeLocalPlayer()
{
    if (!hasLocalPlayer_)
    {
        return;
    }
    upsertEntity(localPlayer_);
}

void EntityRenderer::applySnapshot(const net::EntitySnapshotPayload& snapshot)
{
    for (const auto& entity : snapshot.entities)
    {
        EntityVisual visual;
        visual.entityId = entity.entityId;
        visual.name = entity.name;
        visual.entityType = entity.entityType;
        visual.raceId = entity.raceId;
        visual.classId = entity.classId;
        visual.appearanceId = entity.appearanceId;
        visual.tileX = entity.tileX;
        visual.tileY = entity.tileY;
        visual.isLocalPlayer = hasLocalPlayer_ && entity.entityId == localPlayer_.entityId;
        upsertEntity(std::move(visual));
    }

    mergeLocalPlayer();
}

int EntityRenderer::walkFrameForEntity(const EntityVisual& entity) const
{
    if (entity.tileX != entity.prevTileX || entity.tileY != entity.prevTileY)
    {
        return (entity.tileX + entity.tileY) & 1;
    }
    return 0;
}

SDL_Texture* EntityRenderer::textureForEntity(const EntityVisual& entity, int frameIndex)
{
    if (style_ == nullptr || catalog_ == nullptr)
    {
        return nullptr;
    }

    TextureKey key{
        entity.entityType,
        entity.raceId,
        entity.classId,
        entity.appearanceId,
        frameIndex};
    const auto it = textures_.find(key);
    if (it != textures_.end())
    {
        return it->second;
    }

    const auto pixels = generator_.generateFrame(
        entity.entityType,
        entity.raceId,
        entity.classId,
        entity.appearanceId,
        *style_,
        *catalog_,
        frameIndex);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        const_cast<uint8_t*>(pixels.data()),
        render::SpriteGenerator::kSpriteSize,
        render::SpriteGenerator::kSpriteSize,
        32,
        render::SpriteGenerator::kSpriteSize * 4,
        0x000000FF,
        0x0000FF00,
        0x00FF0000,
        0xFF000000);
    if (surface == nullptr)
    {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (texture != nullptr)
    {
        textures_.emplace(key, texture);
    }
    return texture;
}

void EntityRenderer::render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh)
{
    if (style_ == nullptr || catalog_ == nullptr)
    {
        return;
    }

    std::vector<const EntityVisual*> drawOrder;
    drawOrder.reserve(entities_.size());
    for (const auto& [_, entity] : entities_)
    {
        if (entity.tileX < cameraTileX - 1
            || entity.tileY < cameraTileY - 1
            || entity.tileX > cameraTileX + viewTilesWide
            || entity.tileY > cameraTileY + viewTilesHigh)
        {
            continue;
        }
        drawOrder.push_back(&entity);
    }

    std::sort(
        drawOrder.begin(),
        drawOrder.end(),
        [](const EntityVisual* a, const EntityVisual* b)
        {
            if (a->tileY != b->tileY)
            {
                return a->tileY < b->tileY;
            }
            return a->tileX < b->tileX;
        });

    const int tileSize = render::TileGenerator::kTileSize;
    for (const EntityVisual* entity : drawOrder)
    {
        const int frameIndex = walkFrameForEntity(*entity);
        SDL_Texture* texture = textureForEntity(*entity, frameIndex);
        if (texture == nullptr)
        {
            continue;
        }

        const int screenX = (entity->tileX - cameraTileX) * tileSize;
        const int screenY = (entity->tileY - cameraTileY) * tileSize;
        SDL_Rect dest{
            screenX,
            screenY,
            tileSize,
            tileSize};

        if (entity->isLocalPlayer)
        {
            SDL_SetTextureColorMod(texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(texture, 255);
            SDL_RenderCopy(renderer_, texture, nullptr, &dest);

            SDL_SetRenderDrawColor(renderer_, 240, 240, 120, 255);
            SDL_Rect outline = dest;
            SDL_RenderDrawRect(renderer_, &outline);
            outline.x += 1;
            outline.y += 1;
            outline.w -= 2;
            outline.h -= 2;
            SDL_RenderDrawRect(renderer_, &outline);
        }
        else
        {
            SDL_SetTextureColorMod(texture, 220, 220, 220);
            SDL_SetTextureAlphaMod(texture, 255);
            SDL_RenderCopy(renderer_, texture, nullptr, &dest);
        }
    }
}

std::vector<EntityRenderer::MinimapDot> EntityRenderer::minimapDots() const
{
    std::vector<MinimapDot> dots;
    dots.reserve(entities_.size());
    for (const auto& [_, entity] : entities_)
    {
        dots.push_back(MinimapDot{
            entity.tileX,
            entity.tileY,
            entity.entityType,
            entity.isLocalPlayer});
    }
    return dots;
}

} // namespace tbeq::client
