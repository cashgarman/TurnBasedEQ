#include "GameWindow.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

namespace tbeq::ui
{

namespace
{

ImVec2 clampWindowPosition(const ImVec2& pos, const ImVec2& size, int displayWidth, int displayHeight)
{
    const float maxW = static_cast<float>(displayWidth);
    const float maxH = static_cast<float>(displayHeight);
    const float maxX = std::max(0.0f, maxW - size.x);
    const float maxY = std::max(0.0f, maxH - size.y);

    return ImVec2(
        std::clamp(pos.x, 0.0f, maxX),
        std::clamp(pos.y, 0.0f, maxY));
}

bool isCurrentWindowBeingTitleDragged()
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (g.MovingWindow == nullptr || !g.IO.MouseDown[0])
    {
        return false;
    }

    ImGuiWindow* currentWindow = ImGui::GetCurrentWindow();
    if (currentWindow == nullptr)
    {
        return false;
    }

    ImGuiWindow* movingRoot = g.MovingWindow->RootWindowDockTree;
    ImGuiWindow* currentRoot = currentWindow->RootWindowDockTree;
    return movingRoot == currentRoot && g.ActiveId == g.MovingWindow->MoveId;
}

} // namespace

void constrainActiveImGuiWindowDrag(int displayWidth, int displayHeight)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (g.MovingWindow == nullptr || !g.IO.MouseDown[0])
    {
        return;
    }

    ImGuiWindow* movingWindow = g.MovingWindow->RootWindowDockTree;
    if (movingWindow == nullptr)
    {
        return;
    }

    const ImVec2 desiredPos(
        g.IO.MousePos.x - g.ActiveIdClickOffset.x,
        g.IO.MousePos.y - g.ActiveIdClickOffset.y);
    const ImVec2 clampedPos = clampWindowPosition(desiredPos, movingWindow->Size, displayWidth, displayHeight);

    if (clampedPos.x == desiredPos.x && clampedPos.y == desiredPos.y)
    {
        return;
    }

    g.ActiveIdClickOffset = ImVec2(
        g.IO.MousePos.x - clampedPos.x,
        g.IO.MousePos.y - clampedPos.y);

    if (clampedPos.x != movingWindow->Pos.x || clampedPos.y != movingWindow->Pos.y)
    {
        ImGui::SetWindowPos(movingWindow, clampedPos, ImGuiCond_Always);
    }
}

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
    imguiBegun_ = false;
    if (!state_.visible)
    {
        return false;
    }

    displayWidth_ = displayWidth;
    displayHeight_ = displayHeight;

    clampToDisplay(displayWidth, displayHeight);

    if (pendingLayoutApply_)
    {
        ImGui::SetNextWindowPos(ImVec2(state_.x, state_.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(state_.width, state_.height), ImGuiCond_Always);
        pendingLayoutApply_ = false;
        firstUseApplied_ = true;
    }
    else if (!firstUseApplied_)
    {
        ImGui::SetNextWindowPos(ImVec2(state_.x, state_.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(state_.width, state_.height), ImGuiCond_FirstUseEver);
        firstUseApplied_ = true;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (state_.pinned)
    {
        flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    }

    open_ = state_.visible;
    const std::string windowLabel = state_.title + "##" + state_.id;
    imguiBegun_ = true;
    const bool opened = ImGui::Begin(windowLabel.c_str(), &open_, flags);
    state_.visible = open_;

    return opened;
}

void GameWindow::end()
{
    if (imguiBegun_)
    {
        ImGui::End();
        imguiBegun_ = false;
    }
}

void GameWindow::applyState(const GameWindowState& state)
{
    state_ = state;
    state_.width = std::max(state_.width, minWidth_);
    state_.height = std::max(state_.height, minHeight_);
    pendingLayoutApply_ = true;
}

bool GameWindow::syncFromImGui()
{
    if (!imguiBegun_ || !state_.visible)
    {
        return false;
    }

    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    state_.x = pos.x;
    state_.y = pos.y;
    state_.width = std::max(size.x, minWidth_);
    state_.height = std::max(size.y, minHeight_);

    const GameWindowState before = state_;
    clampToDisplay(displayWidth_, displayHeight_);

    if (isCurrentWindowBeingTitleDragged())
    {
        return state_.x != before.x ||
            state_.y != before.y ||
            state_.width != before.width ||
            state_.height != before.height;
    }

    if (state_.x != pos.x || state_.y != pos.y)
    {
        ImGui::SetWindowPos(ImVec2(state_.x, state_.y), ImGuiCond_Always);
    }
    if (state_.width != size.x || state_.height != size.y)
    {
        ImGui::SetWindowSize(ImVec2(state_.width, state_.height), ImGuiCond_Always);
    }

    return state_.x != before.x ||
        state_.y != before.y ||
        state_.width != before.width ||
        state_.height != before.height;
}

void GameWindow::clampToDisplay(int displayWidth, int displayHeight)
{
    const float maxW = static_cast<float>(displayWidth);
    const float maxH = static_cast<float>(displayHeight);

    state_.width = std::min(std::max(state_.width, minWidth_), maxW);
    state_.height = std::min(std::max(state_.height, minHeight_), maxH);

    const float maxX = maxW - state_.width;
    const float maxY = maxH - state_.height;

    state_.x = std::clamp(state_.x, 0.0f, maxX);
    state_.y = std::clamp(state_.y, 0.0f, maxY);
}

} // namespace tbeq::ui
