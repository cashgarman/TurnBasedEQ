#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "procedural/SpriteGenerator.hpp"
#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::client
{

class EntityRenderer
{
public:
    explicit EntityRenderer(SDL_Renderer* renderer);
    ~EntityRenderer();

    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    void setStyleCatalog(const render::TileStyleProfile* style);
    void setSpriteCatalog(const render::EntitySpriteCatalog* catalog);
    void setItemCatalog(const content::ItemCatalog* catalog);
    void clear();

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
        bool isLocalPlayer = false;
    };

    struct TextureKey
    {
        uint8_t entityType = 0;
        std::string raceId;
        std::string classId;
        std::string appearanceId;
        int frameIndex = 0;
        std::string equippedWeaponItemId;
        std::string equippedHeadItemId;
        std::string equippedChestItemId;
        std::string equippedHandsItemId;

        bool operator==(const TextureKey& other) const
        {
            return entityType == other.entityType
                && raceId == other.raceId
                && classId == other.classId
                && appearanceId == other.appearanceId
                && frameIndex == other.frameIndex
                && equippedWeaponItemId == other.equippedWeaponItemId
                && equippedHeadItemId == other.equippedHeadItemId
                && equippedChestItemId == other.equippedChestItemId
                && equippedHandsItemId == other.equippedHandsItemId;
        }
    };

    struct TextureKeyHash
    {
        std::size_t operator()(const TextureKey& key) const
        {
            return std::hash<std::string>{}(key.raceId)
                ^ (std::hash<std::string>{}(key.classId) << 1)
                ^ (std::hash<std::string>{}(key.appearanceId) << 2)
                ^ (std::hash<std::string>{}(key.equippedWeaponItemId) << 3)
                ^ (std::hash<std::string>{}(key.equippedHeadItemId) << 4)
                ^ (std::hash<std::string>{}(key.equippedChestItemId) << 5)
                ^ (std::hash<std::string>{}(key.equippedHandsItemId) << 6)
                ^ (static_cast<std::size_t>(key.entityType) << 7)
                ^ (static_cast<std::size_t>(key.frameIndex) << 8);
        }
    };

    int walkFrameForEntity(const EntityVisual& entity) const;
    SDL_Texture* textureForEntity(const EntityVisual& entity, int frameIndex);
    void upsertEntity(EntityVisual entity);
    void mergeLocalPlayer();

    SDL_Renderer* renderer_ = nullptr;
    const render::TileStyleProfile* style_ = nullptr;
    const render::EntitySpriteCatalog* catalog_ = nullptr;
    const content::ItemCatalog* itemCatalog_ = nullptr;
    render::SpriteGenerator generator_;
    std::unordered_map<uint32_t, EntityVisual> entities_;
    EntityVisual localPlayer_;
    bool hasLocalPlayer_ = false;
    std::unordered_map<TextureKey, SDL_Texture*, TextureKeyHash> textures_;
};

} // namespace tbeq::client
