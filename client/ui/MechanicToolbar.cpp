#include "MechanicToolbar.hpp"

#include <algorithm>
#include <string>

#include <imgui.h>

namespace tbeq::client
{

namespace
{

std::string buttonLabel(const MechanicToolbarButton& button)
{
    if (button.shortcut == nullptr || button.shortcut[0] == '\0')
    {
        return button.label;
    }

    return std::string(button.label) + " (" + button.shortcut + ")";
}

float measureButtonsWidth(const std::vector<MechanicToolbarButton>& buttons, float spacing)
{
    float totalWidth = 0.0f;
    bool first = true;
    for (const auto& button : buttons)
    {
        if (!button.enabled)
        {
            continue;
        }

        if (!first)
        {
            totalWidth += spacing;
        }
        first = false;

        const std::string text = buttonLabel(button);
        totalWidth += ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    }
    return totalWidth;
}

} // namespace

void MechanicToolbar::draw(int screenWidth, const std::vector<MechanicToolbarButton>& buttons)
{
    constexpr float kButtonSpacing = 6.0f;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenWidth), kHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kButtonSpacing, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.25f, 0.6f));

    ImGui::Begin(
        "##MechanicToolbar",
        nullptr,
        ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus);

    const float totalWidth = measureButtonsWidth(buttons, kButtonSpacing);
    const float startX = std::max(8.0f, (static_cast<float>(screenWidth) - totalWidth) * 0.5f);
    ImGui::SetCursorPosX(startX);

    bool firstButton = true;
    for (const auto& button : buttons)
    {
        if (!button.enabled)
        {
            continue;
        }

        if (!firstButton)
        {
            ImGui::SameLine();
        }
        firstButton = false;

        const std::string text = buttonLabel(button);
        if (button.active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.52f, 0.74f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.40f, 0.58f, 1.0f));
        }

        if (ImGui::Button(text.c_str()) && button.onPress)
        {
            button.onPress();
        }

        if (button.active)
        {
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace tbeq::client
