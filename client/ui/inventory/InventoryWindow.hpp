#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <SDL.h>

#include "render/procedural/ItemIconGenerator.hpp"
#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::ui
{
class GameWindow;
}

namespace tbeq::client
{
class ZoneClient;
}

namespace tbeq::client
{

class InventoryWindow
{
public:
    void setItemCatalog(const content::ItemCatalog* catalog);
    void applySnapshot(const net::InventorySnapshotPayload& snapshot);
    void draw(
        tbeq::ui::GameWindow& window,
        bool& visible,
        ZoneClient* zoneClient,
        SDL_Renderer* renderer,
        int displayWidth,
        int displayHeight,
        const std::function<void(const std::string& line)>& appendLogLine);

private:
    SDL_Texture* iconTexture(const std::string& itemId);
    std::string equippedItemInSlot(const std::string& slot) const;
    uint16_t itemQuantity(const std::string& itemId) const;

    const content::ItemCatalog* itemCatalog_ = nullptr;
    net::InventorySnapshotPayload snapshot_;
    render::ItemIconGenerator iconGenerator_;
    std::unordered_map<std::string, SDL_Texture*> iconTextures_;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace tbeq::client
