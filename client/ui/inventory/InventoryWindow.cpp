#include "inventory/InventoryWindow.hpp"

#include <imgui.h>

#include "net/ZoneClient.hpp"
#include "ui/GameWindow.hpp"

namespace tbeq::client
{

namespace
{

const char* slotLabel(const std::string& slot)
{
    if (slot == "weapon")
    {
        return "Weapon";
    }
    if (slot == "head")
    {
        return "Head";
    }
    if (slot == "chest")
    {
        return "Chest";
    }
    if (slot == "hands")
    {
        return "Hands";
    }
    return slot.c_str();
}

} // namespace

void InventoryWindow::setItemCatalog(const content::ItemCatalog* catalog)
{
    itemCatalog_ = catalog;
}

void InventoryWindow::applySnapshot(const net::InventorySnapshotPayload& snapshot)
{
    snapshot_ = snapshot;
}

std::string InventoryWindow::equippedItemInSlot(const std::string& slot) const
{
    for (const auto& entry : snapshot_.equipment)
    {
        if (entry.slot == slot)
        {
            return entry.itemId;
        }
    }
    return {};
}

uint16_t InventoryWindow::itemQuantity(const std::string& itemId) const
{
    for (const auto& entry : snapshot_.items)
    {
        if (entry.itemId == itemId)
        {
            return entry.quantity;
        }
    }
    return 0;
}

SDL_Texture* InventoryWindow::iconTexture(const std::string& itemId)
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

void InventoryWindow::draw(
    tbeq::ui::GameWindow& window,
    bool& visible,
    ZoneClient* zoneClient,
    SDL_Renderer* renderer,
    int displayWidth,
    int displayHeight,
    const std::function<void(const std::string& line)>& appendLogLine)
{
    renderer_ = renderer;
    if (!visible)
    {
        return;
    }

    window.state().visible = true;
    const bool drawContent = window.begin(displayWidth, displayHeight);
    if (!drawContent)
    {
        window.end();
        return;
    }

    ImGui::Text("Gold: %u", snapshot_.gold);
    ImGui::Separator();
    ImGui::TextUnformatted("Equipment");

    static const char* kSlots[] = {"weapon", "head", "chest", "hands"};
    for (const char* slot : kSlots)
    {
        const std::string equippedId = equippedItemInSlot(slot);
        const content::ItemDef* item = (equippedId.empty() || itemCatalog_ == nullptr)
            ? nullptr
            : itemCatalog_->findItem(equippedId);
        const std::string label = item != nullptr ? item->name : "(empty)";
        ImGui::Text("%s: %s", slotLabel(slot), label.c_str());
        if (!equippedId.empty() && zoneClient != nullptr)
        {
            ImGui::SameLine();
            const std::string buttonLabel = std::string("Unequip ") + slot;
            if (ImGui::SmallButton(buttonLabel.c_str()))
            {
                net::UnequipItemResultPayload result;
                if (zoneClient->unequipItem(slot, result))
                {
                    appendLogLine("[Inventory] " + result.message);
                }
                else
                {
                    appendLogLine("[Inventory] Unequip failed: " + result.message);
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Items");
    ImGui::BeginChild("InventoryItems", ImVec2(0, 0), true);
    for (const auto& entry : snapshot_.items)
    {
        const content::ItemDef* item = itemCatalog_ != nullptr ? itemCatalog_->findItem(entry.itemId) : nullptr;
        const std::string name = item != nullptr ? item->name : entry.itemId;
        SDL_Texture* icon = iconTexture(entry.itemId);
        if (icon != nullptr)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(16.0f, 16.0f));
            ImGui::SameLine();
        }

        ImGui::Text("%s x%u", name.c_str(), entry.quantity);
        if (item != nullptr && item->slot != content::ItemSlot::None && zoneClient != nullptr)
        {
            ImGui::SameLine();
            const std::string buttonLabel = "Equip " + entry.itemId;
            if (ImGui::SmallButton(buttonLabel.c_str()))
            {
                net::EquipItemResultPayload result;
                if (zoneClient->equipItem(entry.itemId, result))
                {
                    appendLogLine("[Inventory] " + result.message);
                }
                else
                {
                    appendLogLine("[Inventory] Equip failed: " + result.message);
                }
            }
        }
    }
    ImGui::EndChild();

    window.end();
}

} // namespace tbeq::client
