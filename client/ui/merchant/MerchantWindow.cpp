#include "merchant/MerchantWindow.hpp"

#include <imgui.h>

#include "net/ZoneClient.hpp"
#include "ui/GameWindow.hpp"

namespace tbeq::client
{

void MerchantWindow::setItemCatalog(const content::ItemCatalog* catalog)
{
    itemCatalog_ = catalog;
}

void MerchantWindow::applyOpen(const net::MerchantOpenPayload& open)
{
    npcEntityId_ = open.npcEntityId;
    npcName_ = open.npcName;
    stock_ = open.stock;
    sellItemIds_ = open.sellItemIds;
}

void MerchantWindow::applyStockUpdate(const net::MerchantBuyResultPayload& result)
{
    if (!result.stockUpdated || result.stockItemId.empty())
    {
        return;
    }

    for (auto& entry : stock_)
    {
        if (entry.itemId == result.stockItemId)
        {
            entry.quantity = result.stockQuantity;
            break;
        }
    }
}

void MerchantWindow::clear()
{
    npcEntityId_ = 0;
    npcName_.clear();
    stock_.clear();
    sellItemIds_.clear();
}

uint16_t MerchantWindow::inventoryQuantity(
    const net::InventorySnapshotPayload& inventory,
    const std::string& itemId) const
{
    for (const auto& entry : inventory.items)
    {
        if (entry.itemId == itemId)
        {
            return entry.quantity;
        }
    }
    return 0;
}

SDL_Texture* MerchantWindow::iconTexture(const std::string& itemId)
{
    if (renderer_ == nullptr || itemCatalog_ == nullptr || itemId.empty())
    {
        return nullptr;
    }

    const auto cached = iconTextures_.find(itemId);
    if (cached != iconTextures_.end())
    {
        return cached->second;
    }

    const content::ItemDef* item = itemCatalog_->findItem(itemId);
    if (item == nullptr)
    {
        return nullptr;
    }

    const auto pixels = iconGenerator_.generate(item->iconShape, item->spriteTint);
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        const_cast<uint8_t*>(pixels.data()),
        render::ItemIconGenerator::kIconSize,
        render::ItemIconGenerator::kIconSize,
        32,
        render::ItemIconGenerator::kIconSize * 4,
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
    if (texture != nullptr)
    {
        iconTextures_.emplace(itemId, texture);
    }
    return texture;
}

void MerchantWindow::draw(
    tbeq::ui::GameWindow& window,
    bool& visible,
    ZoneClient* zoneClient,
    SDL_Renderer* renderer,
    const net::InventorySnapshotPayload& inventory,
    int displayWidth,
    int displayHeight,
    const std::function<void(const std::string& line)>& appendLogLine)
{
    renderer_ = renderer;
    if (!visible || npcEntityId_ == 0)
    {
        visible = false;
        window.state().visible = false;
        return;
    }

    const bool drawContent = window.begin(displayWidth, displayHeight);
    visible = window.state().visible;
    if (!visible)
    {
        clear();
        window.end();
        return;
    }
    if (!drawContent)
    {
        window.end();
        return;
    }

    ImGui::Text("%s", npcName_.c_str());
    ImGui::Text("Your gold: %u", inventory.gold);
    ImGui::Separator();

    if (ImGui::BeginTabBar("MerchantTabs"))
    {
        if (ImGui::BeginTabItem("Buy"))
        {
            for (const auto& entry : stock_)
            {
                const content::ItemDef* item = itemCatalog_ != nullptr ? itemCatalog_->findItem(entry.itemId) : nullptr;
                const std::string name = item != nullptr ? item->name : entry.itemId;
                SDL_Texture* icon = iconTexture(entry.itemId);
                if (icon != nullptr)
                {
                    ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(16.0f, 16.0f));
                    ImGui::SameLine();
                }

                ImGui::Text("%s - %u gp (stock %u)", name.c_str(), entry.buyPrice, entry.quantity);
                if (zoneClient != nullptr && entry.quantity > 0)
                {
                    ImGui::SameLine();
                    const std::string buttonLabel = "Buy " + entry.itemId;
                    if (ImGui::SmallButton(buttonLabel.c_str()))
                    {
                        net::MerchantBuyResultPayload result;
                        if (zoneClient->merchantBuy(npcEntityId_, entry.itemId, 1, result))
                        {
                            applyStockUpdate(result);
                            appendLogLine("[Merchant] " + result.message);
                        }
                        else
                        {
                            appendLogLine("[Merchant] Buy failed: " + result.message);
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sell"))
        {
            for (const auto& itemId : sellItemIds_)
            {
                const uint16_t quantity = inventoryQuantity(inventory, itemId);
                if (quantity == 0)
                {
                    continue;
                }

                const content::ItemDef* item = itemCatalog_ != nullptr ? itemCatalog_->findItem(itemId) : nullptr;
                const std::string name = item != nullptr ? item->name : itemId;
                SDL_Texture* icon = iconTexture(itemId);
                if (icon != nullptr)
                {
                    ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(16.0f, 16.0f));
                    ImGui::SameLine();
                }

                ImGui::Text("%s x%u", name.c_str(), quantity);
                if (zoneClient != nullptr)
                {
                    ImGui::SameLine();
                    const std::string buttonLabel = "Sell " + itemId;
                    if (ImGui::SmallButton(buttonLabel.c_str()))
                    {
                        net::MerchantSellResultPayload result;
                        if (zoneClient->merchantSell(npcEntityId_, itemId, 1, result))
                        {
                            appendLogLine("[Merchant] " + result.message);
                        }
                        else
                        {
                            appendLogLine("[Merchant] Sell failed: " + result.message);
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (ImGui::Button("Close"))
    {
        visible = false;
        window.state().visible = false;
        clear();
    }

    window.end();
    visible = window.state().visible;
    if (!visible)
    {
        clear();
    }
}

} // namespace tbeq::client
