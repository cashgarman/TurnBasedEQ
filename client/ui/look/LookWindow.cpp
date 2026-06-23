#include "look/LookWindow.hpp"

#include <imgui.h>

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

void LookWindow::setItemCatalog(const content::ItemCatalog* catalog)
{
    itemCatalog_ = catalog;
}

void LookWindow::setCharacterInfo(const std::string& name, const std::string& raceId, const std::string& classId)
{
    characterName_ = name;
    raceId_ = raceId;
    classId_ = classId;
}

void LookWindow::applySnapshot(const net::InventorySnapshotPayload& snapshot)
{
    snapshot_ = snapshot;
}

std::string LookWindow::equippedItemInSlot(const std::string& slot) const
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

SDL_Texture* LookWindow::iconTexture(const std::string& itemId)
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

void LookWindow::draw(
    tbeq::ui::GameWindow& window,
    bool& visible,
    SDL_Renderer* renderer,
    int displayWidth,
    int displayHeight)
{
    renderer_ = renderer;
    if (!visible)
    {
        window.state().visible = false;
        return;
    }

    const bool drawContent = window.begin(displayWidth, displayHeight);
    visible = window.state().visible;
    if (!visible)
    {
        window.end();
        return;
    }
    if (!drawContent)
    {
        window.end();
        return;
    }

    ImGui::Text("%s", characterName_.empty() ? "Your Character" : characterName_.c_str());
    if (!raceId_.empty() || !classId_.empty())
    {
        ImGui::Text("%s %s", raceId_.c_str(), classId_.c_str());
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Equipped Gear");

    static const char* kSlots[] = {"weapon", "head", "chest", "hands"};
    int offenseBonus = 0;
    int defenseBonus = 0;
    int hpBonus = 0;
    int manaBonus = 0;

    for (const char* slot : kSlots)
    {
        const std::string equippedId = equippedItemInSlot(slot);
        const content::ItemDef* item = (equippedId.empty() || itemCatalog_ == nullptr)
            ? nullptr
            : itemCatalog_->findItem(equippedId);
        const std::string label = item != nullptr ? item->name : "(empty)";

        SDL_Texture* icon = iconTexture(equippedId);
        if (icon != nullptr)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(16.0f, 16.0f));
            ImGui::SameLine();
        }

        ImGui::Text("%s: %s", slotLabel(slot), label.c_str());
        if (item != nullptr)
        {
            offenseBonus += item->stats.offense;
            defenseBonus += item->stats.defense;
            hpBonus += item->stats.hp;
            manaBonus += item->stats.mana;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Gear Bonuses");
    ImGui::Text("Offense +%d", offenseBonus);
    ImGui::Text("Defense +%d", defenseBonus);
    ImGui::Text("HP +%d", hpBonus);
    ImGui::Text("Mana +%d", manaBonus);

    window.end();
    visible = window.state().visible;
}

} // namespace tbeq::client
