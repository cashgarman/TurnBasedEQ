#include "GameWindow.hpp"

#include <algorithm>

#include <imgui.h>

namespace tbeq::ui
{

GameWindow::GameWindow(std::string id, std::string title, float minWidth, float minHeight)
    : state_{std::move(id), std::move(title), 100.0f, 100.0f, minWidth, minHeight, true, false}
    , minWidth_(minWidth)
    , minHeight_(minHeight)
{
    state_.width = std::max(state_.width, minWidth_);
    state_.height = std::max(state_.height, minHeight_);
}

bool GameWindow::begin(int displayWidth, int displayHeight)
{
    if (!state_.visible)
    {
        return false;
    }

    clampToDisplay(displayWidth, displayHeight);

    ImGui::SetNextWindowPos(ImVec2(state_.x, state_.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(state_.width, state_.height), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (state_.pinned)
    {
        flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    }

    open_ = state_.visible;
    const bool opened = ImGui::Begin(state_.title.c_str(), &open_, flags);
    state_.visible = open_;

    if (opened)
    {
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        state_.x = pos.x;
        state_.y = pos.y;
        state_.width = std::max(size.x, minWidth_);
        state_.height = std::max(size.y, minHeight_);
    }

    return opened;
}

void GameWindow::end()
{
    ImGui::End();
}

void GameWindow::applyState(const GameWindowState& state)
{
    state_ = state;
    state_.width = std::max(state_.width, minWidth_);
    state_.height = std::max(state_.height, minHeight_);
}

void GameWindow::clampToDisplay(int displayWidth, int displayHeight)
{
    const float minVisibleWidth = state_.width * 0.5f;
    const float minVisibleHeight = state_.height * 0.5f;

    const float maxX = static_cast<float>(displayWidth) - minVisibleWidth;
    const float maxY = static_cast<float>(displayHeight) - minVisibleHeight;

    state_.x = std::clamp(state_.x, -minVisibleWidth, maxX);
    state_.y = std::clamp(state_.y, 0.0f, maxY);
    state_.width = std::clamp(state_.width, minWidth_, static_cast<float>(displayWidth));
    state_.height = std::clamp(state_.height, minHeight_, static_cast<float>(displayHeight));
}

} // namespace tbeq::ui
