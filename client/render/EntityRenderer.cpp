#include "render/EntityRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace tbeq::client
{

EntityRenderer::EntityRenderer(SpriteRenderer& sprites)
    : sprites_(sprites)
{
}

EntityRenderer::~EntityRenderer() = default;

void EntityRenderer::setStyleCatalog(const render::TileStyleProfile* style)
{
    style_ = style;
    sprites_.setStyle(style);
}

void EntityRenderer::setSpriteCatalog(const render::EntitySpriteCatalog* catalog)
{
    catalog_ = catalog;
    sprites_.setSpriteCatalog(catalog);
}

void EntityRenderer::setItemCatalog(const content::ItemCatalog* catalog)
{
    itemCatalog_ = catalog;
    sprites_.setItemCatalog(catalog);
}

void EntityRenderer::clear()
{
    entities_.clear();
    hasLocalPlayer_ = false;
}

void EntityRenderer::updateAnimation(uint64_t tickMs)
{
    animTickMs_ = tickMs;
}

void EntityRenderer::upsertEntity(EntityVisual entity)
{
    const auto it = entities_.find(entity.entityId);
    if (it != entities_.end())
    {
        entity.prevTileX = it->second.tileX;
        entity.prevTileY = it->second.tileY;
        entity.facing = it->second.facing;
    }
    else
    {
        entity.prevTileX = entity.tileX;
        entity.prevTileY = entity.tileY;
    }

    if (entity.tileX != entity.prevTileX || entity.tileY != entity.prevTileY)
    {
        entity.facing = render::facingFromDelta(
            entity.tileX - entity.prevTileX,
            entity.tileY - entity.prevTileY);
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
    localPlayer_.facing = render::Facing::South;
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
    if (tileX != localPlayer_.prevTileX || tileY != localPlayer_.prevTileY)
    {
        localPlayer_.facing = render::facingFromDelta(
            tileX - localPlayer_.prevTileX,
            tileY - localPlayer_.prevTileY);
    }
    mergeLocalPlayer();
}

void EntityRenderer::setLocalPlayerEquipment(
    const std::string& weaponItemId,
    const std::string& headItemId,
    const std::string& chestItemId,
    const std::string& handsItemId)
{
    if (!hasLocalPlayer_)
    {
        return;
    }

    localPlayer_.equippedWeaponItemId = weaponItemId;
    localPlayer_.equippedHeadItemId = headItemId;
    localPlayer_.equippedChestItemId = chestItemId;
    localPlayer_.equippedHandsItemId = handsItemId;
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
    std::unordered_map<uint32_t, EntityVisual> preserved;
    preserved.swap(entities_);

    for (const auto& entity : snapshot.entities)
    {
        EntityVisual visual;
        visual.entityId = entity.entityId;
        visual.name = entity.name;
        visual.entityType = entity.entityType;
        visual.raceId = entity.raceId;
        visual.classId = entity.classId;
        visual.appearanceId = entity.appearanceId;
        visual.equippedWeaponItemId = entity.equippedWeaponItemId;
        visual.equippedHeadItemId = entity.equippedHeadItemId;
        visual.equippedChestItemId = entity.equippedChestItemId;
        visual.equippedHandsItemId = entity.equippedHandsItemId;
        visual.tileX = entity.tileX;
        visual.tileY = entity.tileY;
        visual.isLocalPlayer = hasLocalPlayer_ && entity.entityId == localPlayer_.entityId;

        if (visual.isLocalPlayer)
        {
            localPlayer_.prevTileX = localPlayer_.tileX;
            localPlayer_.prevTileY = localPlayer_.tileY;
            localPlayer_.tileX = entity.tileX;
            localPlayer_.tileY = entity.tileY;
            visual.prevTileX = localPlayer_.prevTileX;
            visual.prevTileY = localPlayer_.prevTileY;
            if (localPlayer_.tileX != localPlayer_.prevTileX || localPlayer_.tileY != localPlayer_.prevTileY)
            {
                localPlayer_.facing = render::facingFromDelta(
                    localPlayer_.tileX - localPlayer_.prevTileX,
                    localPlayer_.tileY - localPlayer_.prevTileY);
            }
            visual.facing = localPlayer_.facing;
        }
        else
        {
            const auto prev = preserved.find(entity.entityId);
            if (prev != preserved.end())
            {
                visual.prevTileX = prev->second.tileX;
                visual.prevTileY = prev->second.tileY;
                visual.facing = prev->second.facing;
            }
            else
            {
                visual.prevTileX = entity.tileX;
                visual.prevTileY = entity.tileY;
            }

            if (visual.tileX != visual.prevTileX || visual.tileY != visual.prevTileY)
            {
                visual.facing = render::facingFromDelta(
                    visual.tileX - visual.prevTileX,
                    visual.tileY - visual.prevTileY);
            }
        }

        entities_[entity.entityId] = std::move(visual);
    }

    mergeLocalPlayer();
}

render::AnimationClipId EntityRenderer::clipForEntity(const EntityVisual& entity) const
{
    if (entity.tileX != entity.prevTileX || entity.tileY != entity.prevTileY)
    {
        return render::AnimationClipId::Walk;
    }
    return render::AnimationClipId::Idle;
}

int EntityRenderer::frameIndexForEntity(const EntityVisual& entity) const
{
    const render::AnimationClipId clip = clipForEntity(entity);
    const int phaseOffset = static_cast<int>(entity.entityId % 4);
    return render::frameIndexForClip(animTickMs_, clip, phaseOffset);
}

EntitySpriteRequest EntityRenderer::spriteRequestForEntity(const EntityVisual& entity) const
{
    EntitySpriteRequest request;
    request.entityType = entity.entityType;
    request.raceId = entity.raceId;
    request.classId = entity.classId;
    request.appearanceId = entity.appearanceId;
    request.clipId = clipForEntity(entity);
    request.facing = entity.facing;
    request.frameIndex = frameIndexForEntity(entity);
    request.equippedWeaponItemId = entity.equippedWeaponItemId;
    request.equippedHeadItemId = entity.equippedHeadItemId;
    request.equippedChestItemId = entity.equippedChestItemId;
    request.equippedHandsItemId = entity.equippedHandsItemId;
    return request;
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

    SDL_Renderer* renderer = sprites_.cache().renderer();
    const int tileSize = render::TileGenerator::kTileSize;
    for (const EntityVisual* entity : drawOrder)
    {
        SDL_Texture* texture = sprites_.textureForEntity(spriteRequestForEntity(*entity));
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

        SDL_RenderCopy(renderer, texture, nullptr, &dest);

        if (!drawEntityOutlines_)
        {
            continue;
        }

        if (entity->isLocalPlayer)
        {
            SDL_SetRenderDrawColor(renderer, 240, 240, 120, 255);
            SDL_Rect outline = dest;
            SDL_RenderDrawRect(renderer, &outline);
            outline.x += 1;
            outline.y += 1;
            outline.w -= 2;
            outline.h -= 2;
            SDL_RenderDrawRect(renderer, &outline);
        }
        else if (entity->entityType == 1)
        {
            if (entity->appearanceId == "merchant")
            {
                SDL_SetRenderDrawColor(renderer, 255, 210, 80, 255);
            }
            else if (entity->appearanceId == "lore")
            {
                SDL_SetRenderDrawColor(renderer, 140, 180, 255, 255);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 200, 160, 255, 255);
            }

            SDL_Rect outline = dest;
            SDL_RenderDrawRect(renderer, &outline);
            outline.x += 1;
            outline.y += 1;
            outline.w -= 2;
            outline.h -= 2;
            SDL_RenderDrawRect(renderer, &outline);
        }
        else if (entity->entityType == 2)
        {
            SDL_SetRenderDrawColor(renderer, 220, 80, 80, 255);
            SDL_Rect outline = dest;
            SDL_RenderDrawRect(renderer, &outline);
            outline.x += 1;
            outline.y += 1;
            outline.w -= 2;
            outline.h -= 2;
            SDL_RenderDrawRect(renderer, &outline);
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

namespace
{

std::string roleLabelForAppearance(const std::string& appearanceId)
{
    if (appearanceId == "merchant")
    {
        return "Merchant";
    }
    if (appearanceId == "lore")
    {
        return "Lorekeeper";
    }
    if (appearanceId == "quest")
    {
        return "Quest Giver";
    }
    return "NPC";
}

} // namespace

std::vector<EntityRenderer::WorldNameplate> EntityRenderer::visibleNameplates(
    int cameraTileX,
    int cameraTileY,
    int viewTilesWide,
    int viewTilesHigh) const
{
    std::vector<WorldNameplate> plates;
    for (const auto& [_, entity] : entities_)
    {
        if (entity.entityType == 0 || entity.isLocalPlayer)
        {
            continue;
        }
        if (entity.tileX < cameraTileX - 1
            || entity.tileY < cameraTileY - 1
            || entity.tileX > cameraTileX + viewTilesWide
            || entity.tileY > cameraTileY + viewTilesHigh)
        {
            continue;
        }

        WorldNameplate plate;
        plate.tileX = entity.tileX;
        plate.tileY = entity.tileY;
        if (entity.entityType == 2)
        {
            plate.name = entity.name.empty() ? entity.appearanceId : entity.name;
            plate.role = "Mob";
        }
        else
        {
            plate.name = entity.name.empty() ? roleLabelForAppearance(entity.appearanceId) : entity.name;
            plate.role = roleLabelForAppearance(entity.appearanceId);
        }
        plate.appearanceId = entity.appearanceId;
        plates.push_back(std::move(plate));
    }
    return plates;
}

std::optional<EntityRenderer::WorldNameplate> EntityRenderer::nearestNpcNameplate(
    int32_t tileX,
    int32_t tileY,
    int32_t maxDistance) const
{
    const auto entityId = nearestNpcEntity(tileX, tileY, maxDistance);
    if (!entityId.has_value())
    {
        return std::nullopt;
    }

    const auto it = entities_.find(*entityId);
    if (it == entities_.end())
    {
        return std::nullopt;
    }

    WorldNameplate plate;
    plate.tileX = it->second.tileX;
    plate.tileY = it->second.tileY;
    plate.name = it->second.name.empty()
        ? roleLabelForAppearance(it->second.appearanceId)
        : it->second.name;
    plate.role = roleLabelForAppearance(it->second.appearanceId);
    plate.appearanceId = it->second.appearanceId;
    return plate;
}

std::optional<uint32_t> EntityRenderer::npcEntityAtTile(int32_t tileX, int32_t tileY) const
{
    for (const auto& [_, entity] : entities_)
    {
        if (entity.entityType == 1 && entity.tileX == tileX && entity.tileY == tileY)
        {
            return entity.entityId;
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> EntityRenderer::nearestNpcEntity(
    int32_t tileX,
    int32_t tileY,
    int32_t maxDistance) const
{
    uint32_t bestEntityId = 0;
    int32_t bestDistance = maxDistance + 1;

    for (const auto& [_, entity] : entities_)
    {
        if (entity.entityType != 1)
        {
            continue;
        }

        const int32_t distance = std::abs(entity.tileX - tileX) + std::abs(entity.tileY - tileY);
        if (distance <= maxDistance && distance < bestDistance)
        {
            bestDistance = distance;
            bestEntityId = entity.entityId;
        }
    }

    if (bestEntityId == 0)
    {
        return std::nullopt;
    }
    return bestEntityId;
}

} // namespace tbeq::client
