#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>

#include <SDL.h>

namespace tbeq::client
{

class TextureCache
{
public:
    static constexpr std::size_t kDefaultMaxEntries = 512;

    explicit TextureCache(SDL_Renderer* renderer, std::size_t maxEntries = kDefaultMaxEntries);
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    SDL_Texture* getOrCreate(uint64_t key, const std::function<SDL_Texture*()>& factory);
    SDL_Texture* find(uint64_t key) const;
    void clear();

    SDL_Texture* createFromRgbaPixels(const uint8_t* pixels, int width, int height);

    SDL_Renderer* renderer() const { return renderer_; }

    std::size_t size() const { return entries_.size(); }

private:
    void touch(std::list<uint64_t>::iterator lruIt);
    void evictIfNeeded();

    SDL_Renderer* renderer_ = nullptr;
    std::size_t maxEntries_ = kDefaultMaxEntries;
    std::unordered_map<uint64_t, SDL_Texture*> entries_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lruIndex_;
    std::list<uint64_t> lruOrder_;
};

uint64_t hashCombine(uint64_t seed, uint64_t value);

} // namespace tbeq::client
