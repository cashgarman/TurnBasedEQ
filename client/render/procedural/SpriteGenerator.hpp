#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "procedural/TileGenerator.hpp"
#include "render/AnimationTypes.hpp"

namespace tbeq::render
{

struct EntityRaceTint
{
    float hueOffset = 0.0f;
    float saturation = 1.0f;
    float lightness = 0.0f;
};

struct EntityClassTint
{
    float accentMix = 0.3f;
    float shadowMix = 0.5f;
};

struct EntityNpcRole
{
    std::string roleColor = "#808080";
    std::string silhouette = "default";
};

struct MobBodyDef
{
    std::string silhouette = "quadruped_small";
    std::string bodyColor = "#808080";
    float scale = 1.0f;
};

struct EntitySpriteCatalog
{
    bool loadFromFile(const std::filesystem::path& path);

    EntityRaceTint raceTint(const std::string& raceId) const;
    EntityClassTint classTint(const std::string& classId) const;
    EntityNpcRole npcRole(const std::string& appearanceId) const;
    MobBodyDef mobBody(const std::string& mobId) const;

private:
    std::unordered_map<std::string, EntityRaceTint> races_;
    std::unordered_map<std::string, EntityClassTint> classes_;
    std::unordered_map<std::string, EntityNpcRole> npcRoles_;
    std::unordered_map<std::string, MobBodyDef> mobBodies_;
    MobBodyDef defaultMobBody_;
    std::string defaultRaceId_ = "human";
    std::string defaultClassId_ = "warrior";
    std::string defaultNpcRole_ = "quest";
};

struct GearLayerTints
{
    std::string weapon;
    std::string head;
    std::string chest;
    std::string hands;
};

class SpriteGenerator
{
public:
    static constexpr int kSpriteSize = 32;

    std::vector<uint8_t> generateFrame(
        uint8_t entityType,
        const std::string& raceId,
        const std::string& classId,
        const std::string& appearanceId,
        const TileStyleProfile& style,
        const EntitySpriteCatalog& catalog,
        AnimationClipId clipId,
        Facing facing,
        int frameIndex,
        const GearLayerTints& gearTints = {}) const;
};

} // namespace tbeq::render
