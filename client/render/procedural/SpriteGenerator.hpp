#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "procedural/TileGenerator.hpp"

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

struct EntitySpriteCatalog
{
    bool loadFromFile(const std::filesystem::path& path);

    EntityRaceTint raceTint(const std::string& raceId) const;
    EntityClassTint classTint(const std::string& classId) const;
    EntityNpcRole npcRole(const std::string& appearanceId) const;

private:
    std::unordered_map<std::string, EntityRaceTint> races_;
    std::unordered_map<std::string, EntityClassTint> classes_;
    std::unordered_map<std::string, EntityNpcRole> npcRoles_;
    std::string defaultRaceId_ = "human";
    std::string defaultClassId_ = "warrior";
    std::string defaultNpcRole_ = "quest";
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
        int frameIndex,
        const std::string& weaponTintHex = "") const;
};

} // namespace tbeq::render
