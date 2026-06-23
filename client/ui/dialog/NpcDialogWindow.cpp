#include "dialog/NpcDialogWindow.hpp"

#include <imgui.h>

#include "ui/GameWindow.hpp"

namespace tbeq::client
{

void NpcDialogWindow::applyOpen(const net::NpcDialogOpenPayload& open)
{
    npcEntityId_ = open.npcEntityId;
    npcName_ = open.npcName;
    lines_ = open.lines;
}

void NpcDialogWindow::clear()
{
    npcEntityId_ = 0;
    npcName_.clear();
    lines_.clear();
}

void NpcDialogWindow::draw(
    tbeq::ui::GameWindow& window,
    bool& visible,
    int displayWidth,
    int displayHeight)
{
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

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
    ImGui::TextUnformatted(npcName_.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImGui::BeginChild("NpcDialogBody", ImVec2(0.0f, 160.0f), true);
    for (const auto& line : lines_)
    {
        ImGui::TextWrapped("%s", line.c_str());
        ImGui::Spacing();
    }
    ImGui::EndChild();

    if (ImGui::Button("Close", ImVec2(120.0f, 0.0f)))
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
