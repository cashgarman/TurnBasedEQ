#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "procedural/SpriteGenerator.hpp"
#include "render/AnimationTypes.hpp"
#include "render/SpriteRenderer.hpp"
#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::client
{

class EntityRenderer
{
public:
    explicit EntityRenderer(SpriteRenderer& sprites);
    ~EntityRenderer();

    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    void setStyleCatalog(const render::TileStyleProfile* style);
    void setSpriteCatalog(const render::EntitySpriteCatalog* catalog);
    void setItemCatalog(const content::ItemCatalog* catalog);
    void clear();

    void updateAnimation(uint64_t tickMs);

    void applySnapshot(const net::EntitySnapshotPayload& snapshot);
    void setLocalPlayer(
        uint32_t entityId,
        const std::string& name,
        const std::string& raceId,
        const std::string& classId,
        int32_t tileX,
        int32_t tileY);
    void updateLocalPlayerPosition(int32_t tileX, int32_t tileY);
    void setLocalPlayerEquipment(
        const std::string& weaponItemId,
        const std::string& headItemId,
        const std::string& chestItemId,
        const std::string& handsItemId);

    void setDrawEntityOutlines(bool draw) { drawEntityOutlines_ = draw; }

    void render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh);

    std::optional<uint32_t> npcEntityAtTile(int32_t tileX, int32_t tileY) const;
    std::optional<uint32_t> nearestNpcEntity(int32_t tileX, int32_t tileY, int32_t maxDistance) const;

    struct MinimapDot
    {
        int32_t tileX = 0;
        int32_t tileY = 0;
        uint8_t entityType = 0;
        bool isLocalPlayer = false;
    };

    std::vector<MinimapDot> minimapDots() const;

    struct WorldNameplate
    {
        int32_t tileX = 0;
        int32_t tileY = 0;
        std::string name;
        std::string role;
        std::string appearanceId;
    };

    std::vector<WorldNameplate> visibleNameplates(
        int cameraTileX,
        int cameraTileY,
        int viewTilesWide,
        int viewTilesHigh) const;

    std::optional<WorldNameplate> nearestNpcNameplate(
        int32_t tileX,
        int32_t tileY,
        int32_t maxDistance) const;

private:
    struct EntityVisual
    {
        uint32_t entityId = 0;
        std::string name;
        uint8_t entityType = 0;
        std::string raceId;
        std::string classId;
        std::string appearanceId;
        std::string equippedWeaponItemId;
        std::string equippedHeadItemId;
        std::string equippedChestItemId;
        std::string equippedHandsItemId;
        int32_t tileX = 0;
        int32_t tileY = 0;
        int32_t prevTileX = 0;
        int32_t prevTileY = 0;
        render::Facing facing = render::Facing::South;
        bool isLocalPlayer = false;
    };

    EntitySpriteRequest spriteRequestForEntity(const EntityVisual& entity) const;
    render::AnimationClipId clipForEntity(const EntityVisual& entity) const;
    int frameIndexForEntity(const EntityVisual& entity) const;
    void upsertEntity(EntityVisual entity);
    void mergeLocalPlayer();

    SpriteRenderer& sprites_;
    const render::TileStyleProfile* style_ = nullptr;
    const render::EntitySpriteCatalog* catalog_ = nullptr;
    const content::ItemCatalog* itemCatalog_ = nullptr;
    std::unordered_map<uint32_t, EntityVisual> entities_;
    EntityVisual localPlayer_;
    bool hasLocalPlayer_ = false;
    uint64_t animTickMs_ = 0;
    bool drawEntityOutlines_ = false;
};

} // namespace tbeq::client
