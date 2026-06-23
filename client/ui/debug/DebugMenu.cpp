#include "DebugMenu.hpp"

#include <imgui.h>

#include "LogViewer.hpp"
#include "UnitTestRunner.hpp"
#include "net/ZoneClient.hpp"
#include "tbeq/net/DebugCommands.hpp"

namespace tbeq::ui
{

DebugMenu::DebugMenu(LogViewer& logViewer)
    : logViewer_(logViewer)
{
}

void DebugMenu::handleInput()
{
}

void DebugMenu::drawCheats(
    tbeq::client::ZoneClient* zoneClient,
    const std::function<void(const std::string& line)>& appendLogLine)
{
    if (zoneClient == nullptr)
    {
        ImGui::TextUnformatted("Enter the world to use zone debug commands.");
        return;
    }

    ImGui::TextUnformatted("Combat cheats (dev-mode server required)");
    if (ImGui::Button("Spawn forest_rat"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::SpawnMob, {"forest_rat"}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn AI Cleric"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::SpawnAi, {"cleric"}, response);
        appendLogLine("[Debug] " + response.message);
    }
    if (ImGui::Button("Fill mana"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::FillMana, {}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Unlock spells"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::UnlockAllSpells, {}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("God mode"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::GodMode, {"on"}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Force combat end"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::ForceCombatEnd, {}, response);
        appendLogLine("[Debug] " + response.message);
    }
}

void DebugMenu::update(
    tbeq::client::ZoneClient* zoneClient,
    const std::function<void(const std::string& line)>& appendLogLine)
{
    unitTestRunner_.update();

    if (ImGui::BeginTabBar("DebugTabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        if (ImGui::BeginTabItem("Log Viewer"))
        {
            logViewer_.draw();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Unit Tests"))
        {
            unitTestRunner_.draw();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cheats"))
        {
            drawCheats(zoneClient, appendLogLine);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace tbeq::ui
