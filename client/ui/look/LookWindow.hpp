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

class LookWindow
{
public:
    void setItemCatalog(const content::ItemCatalog* catalog);
    void setCharacterInfo(const std::string& name, const std::string& raceId, const std::string& classId);
    void applySnapshot(const net::InventorySnapshotPayload& snapshot);
    void draw(
        tbeq::ui::GameWindow& window,
        bool& visible,
        SDL_Renderer* renderer,
        int displayWidth,
        int displayHeight);

private:
    SDL_Texture* iconTexture(const std::string& itemId);
    std::string equippedItemInSlot(const std::string& slot) const;

    const content::ItemCatalog* itemCatalog_ = nullptr;
    std::string characterName_;
    std::string raceId_;
    std::string classId_;
    net::InventorySnapshotPayload snapshot_;
    render::ItemIconGenerator iconGenerator_;
    std::unordered_map<std::string, SDL_Texture*> iconTextures_;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace tbeq::client
