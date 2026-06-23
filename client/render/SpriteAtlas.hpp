#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/AnimationTypes.hpp"

namespace tbeq::render
{

struct AtlasLayout
{
    int frameWidth = 32;
    int frameHeight = 32;
    int idleStartRow = 0;
    int walkStartRow = 4;
};

struct LoadedAtlas
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

class SpriteAtlasLibrary
{
public:
    static constexpr int kSpriteSize = 32;

    void setDataRoot(const std::filesystem::path& dataRoot);
    bool loadAtlas(const std::string& atlasId, const std::filesystem::path& relativePath);

    bool hasAtlas(const std::string& atlasId) const;
    std::vector<uint8_t> framePixels(
        const std::string& atlasId,
        const AtlasLayout& layout,
        AnimationClipId clipId,
        Facing facing,
        int frameIndex) const;

private:
    const LoadedAtlas* findAtlas(const std::string& atlasId) const;
    std::filesystem::path resolvePath(const std::filesystem::path& relativePath) const;

    std::filesystem::path dataRoot_;
    std::unordered_map<std::string, LoadedAtlas> atlases_;
};

} // namespace tbeq::render
