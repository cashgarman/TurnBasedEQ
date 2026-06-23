#include "render/SpriteAtlas.hpp"

#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace tbeq::render
{

void SpriteAtlasLibrary::setDataRoot(const std::filesystem::path& dataRoot)
{
    dataRoot_ = dataRoot;
}

std::filesystem::path SpriteAtlasLibrary::resolvePath(const std::filesystem::path& relativePath) const
{
    if (relativePath.is_absolute())
    {
        return relativePath;
    }
    return dataRoot_ / relativePath;
}

bool SpriteAtlasLibrary::loadAtlas(const std::string& atlasId, const std::filesystem::path& relativePath)
{
    if (atlases_.contains(atlasId))
    {
        return true;
    }

    const std::filesystem::path path = resolvePath(relativePath);
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* loaded = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (loaded == nullptr)
    {
        return false;
    }

    LoadedAtlas atlas;
    atlas.width = width;
    atlas.height = height;
    atlas.pixels.assign(loaded, loaded + static_cast<std::size_t>(width * height * 4));
    stbi_image_free(loaded);
    atlases_.emplace(atlasId, std::move(atlas));
    return true;
}

bool SpriteAtlasLibrary::hasAtlas(const std::string& atlasId) const
{
    return atlases_.contains(atlasId);
}

const LoadedAtlas* SpriteAtlasLibrary::findAtlas(const std::string& atlasId) const
{
    const auto it = atlases_.find(atlasId);
    return it != atlases_.end() ? &it->second : nullptr;
}

std::vector<uint8_t> SpriteAtlasLibrary::framePixels(
    const std::string& atlasId,
    const AtlasLayout& layout,
    AnimationClipId clipId,
    Facing facing,
    int frameIndex) const
{
    const LoadedAtlas* atlas = findAtlas(atlasId);
    std::vector<uint8_t> pixels(static_cast<std::size_t>(layout.frameWidth * layout.frameHeight * 4), 0);
    if (atlas == nullptr || layout.frameWidth <= 0 || layout.frameHeight <= 0)
    {
        return pixels;
    }

    const int facingIndex = static_cast<int>(facing);
    const int clipRowOffset = clipId == AnimationClipId::Walk ? layout.walkStartRow : layout.idleStartRow;
    const int row = clipRowOffset + facingIndex;
    const int col = frameIndex % 4;

    const int srcX = col * layout.frameWidth;
    const int srcY = row * layout.frameHeight;
    if (srcX + layout.frameWidth > atlas->width || srcY + layout.frameHeight > atlas->height)
    {
        return pixels;
    }

    for (int y = 0; y < layout.frameHeight; ++y)
    {
        const std::size_t srcRow = static_cast<std::size_t>((srcY + y) * atlas->width + srcX) * 4;
        const std::size_t dstRow = static_cast<std::size_t>(y * layout.frameWidth) * 4;
        std::copy(
            atlas->pixels.begin() + srcRow,
            atlas->pixels.begin() + srcRow + static_cast<std::size_t>(layout.frameWidth * 4),
            pixels.begin() + dstRow);
    }

    return pixels;
}

} // namespace tbeq::render
