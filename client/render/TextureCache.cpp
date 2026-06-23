#include "render/TextureCache.hpp"

namespace tbeq::client
{

uint64_t hashCombine(uint64_t seed, uint64_t value)
{
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

TextureCache::TextureCache(SDL_Renderer* renderer, std::size_t maxEntries)
    : renderer_(renderer)
    , maxEntries_(maxEntries)
{
}

TextureCache::~TextureCache()
{
    clear();
}

void TextureCache::clear()
{
    for (auto& [_, texture] : entries_)
    {
        SDL_DestroyTexture(texture);
    }
    entries_.clear();
    lruIndex_.clear();
    lruOrder_.clear();
}

SDL_Texture* TextureCache::createFromRgbaPixels(const uint8_t* pixels, int width, int height)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        const_cast<uint8_t*>(pixels),
        width,
        height,
        32,
        width * 4,
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
    return texture;
}

SDL_Texture* TextureCache::find(uint64_t key) const
{
    const auto it = entries_.find(key);
    return it != entries_.end() ? it->second : nullptr;
}

void TextureCache::touch(std::list<uint64_t>::iterator lruIt)
{
    lruOrder_.splice(lruOrder_.end(), lruOrder_, lruIt);
}

void TextureCache::evictIfNeeded()
{
    while (entries_.size() >= maxEntries_ && !lruOrder_.empty())
    {
        const uint64_t evictKey = lruOrder_.front();
        lruOrder_.pop_front();
        const auto entryIt = entries_.find(evictKey);
        if (entryIt != entries_.end())
        {
            SDL_DestroyTexture(entryIt->second);
            entries_.erase(entryIt);
        }
        lruIndex_.erase(evictKey);
    }
}

SDL_Texture* TextureCache::getOrCreate(uint64_t key, const std::function<SDL_Texture*()>& factory)
{
    const auto existing = entries_.find(key);
    if (existing != entries_.end())
    {
        touch(lruIndex_.at(key));
        return existing->second;
    }

    evictIfNeeded();
    SDL_Texture* texture = factory();
    if (texture == nullptr)
    {
        return nullptr;
    }

    entries_.emplace(key, texture);
    lruOrder_.push_back(key);
    lruIndex_.emplace(key, std::prev(lruOrder_.end()));
    return texture;
}

} // namespace tbeq::client
