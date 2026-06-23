#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

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

class MerchantWindow
{
public:
    void setItemCatalog(const content::ItemCatalog* catalog);
    void applyOpen(const net::MerchantOpenPayload& open);
    void clear();
    bool hasMerchant() const { return npcEntityId_ != 0; }
    uint32_t npcEntityId() const { return npcEntityId_; }

    void draw(
        tbeq::ui::GameWindow& window,
        bool& visible,
        ZoneClient* zoneClient,
        SDL_Renderer* renderer,
        const net::InventorySnapshotPayload& inventory,
        int displayWidth,
        int displayHeight,
        const std::function<void(const std::string& line)>& appendLogLine);

private:
    SDL_Texture* iconTexture(const std::string& itemId);
    uint16_t inventoryQuantity(const net::InventorySnapshotPayload& inventory, const std::string& itemId) const;

    const content::ItemCatalog* itemCatalog_ = nullptr;
    uint32_t npcEntityId_ = 0;
    std::string npcName_;
    std::vector<net::MerchantStockEntryPayload> stock_;
    std::vector<std::string> sellItemIds_;
    render::ItemIconGenerator iconGenerator_;
    std::unordered_map<std::string, SDL_Texture*> iconTextures_;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace tbeq::client
