#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "procedural/SpriteGenerator.hpp"
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

    void render(int cameraTileX, int cameraTileY, int viewTilesWide, int viewTilesHigh);

    struct MinimapDot
    {
        int32_t tileX = 0;
        int32_t tileY = 0;
        uint8_t entityType = 0;
        bool isLocalPlayer = false;
    };

    std::vector<MinimapDot> minimapDots() const;

private:
    struct EntityVisual
    {
        uint32_t entityId = 0;
        std::string name;
        uint8_t entityType = 0;
        std::string raceId;
        std::string classId;
        std::string appearanceId;
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

        bool operator==(const TextureKey& other) const
        {
            return entityType == other.entityType
                && raceId == other.raceId
                && classId == other.classId
                && appearanceId == other.appearanceId
                && frameIndex == other.frameIndex;
        }
    };

    struct TextureKeyHash
    {
        std::size_t operator()(const TextureKey& key) const
        {
            return std::hash<std::string>{}(key.raceId)
                ^ (std::hash<std::string>{}(key.classId) << 1)
                ^ (std::hash<std::string>{}(key.appearanceId) << 2)
                ^ (static_cast<std::size_t>(key.entityType) << 3)
                ^ (static_cast<std::size_t>(key.frameIndex) << 4);
        }
    };

    int walkFrameForEntity(const EntityVisual& entity) const;
    SDL_Texture* textureForEntity(const EntityVisual& entity, int frameIndex);
    void upsertEntity(EntityVisual entity);
    void mergeLocalPlayer();

    SDL_Renderer* renderer_ = nullptr;
    const render::TileStyleProfile* style_ = nullptr;
    const render::EntitySpriteCatalog* catalog_ = nullptr;
    render::SpriteGenerator generator_;
    std::unordered_map<uint32_t, EntityVisual> entities_;
    EntityVisual localPlayer_;
    bool hasLocalPlayer_ = false;
    std::unordered_map<TextureKey, SDL_Texture*, TextureKeyHash> textures_;
};

} // namespace tbeq::client
