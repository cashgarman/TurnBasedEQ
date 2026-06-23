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

    ImGui::Separator();
    ImGui::TextUnformatted("Progression cheats");
    static char skillIdBuffer[64] = "offense";
    static char skillLevelBuffer[8] = "50";
    ImGui::InputText("Skill id", skillIdBuffer, sizeof(skillIdBuffer));
    ImGui::InputText("Skill level", skillLevelBuffer, sizeof(skillLevelBuffer));
    if (ImGui::Button("Set skill level"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(
            tbeq::net::DebugCommand::SetSkillLevel,
            {skillIdBuffer, skillLevelBuffer},
            response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Max skills"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::MaxSkills, {}, response);
        appendLogLine("[Debug] " + response.message);
    }
    if (ImGui::Button("Grant 500 XP"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::GrantExperience, {"500"}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Practice forage"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::PracticeSkill, {"forage"}, response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Practice pick lock"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(tbeq::net::DebugCommand::PracticeSkill, {"pick_lock"}, response);
        appendLogLine("[Debug] " + response.message);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Item cheats");
    static char grantItemBuffer[64] = "rusty_dagger";
    ImGui::InputText("Item id", grantItemBuffer, sizeof(grantItemBuffer));
    if (ImGui::Button("Grant item"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(
            tbeq::net::DebugCommand::GrantItem,
            {grantItemBuffer, "1"},
            response);
        appendLogLine("[Debug] " + response.message);
    }
    ImGui::SameLine();
    if (ImGui::Button("Equip item"))
    {
        tbeq::net::DebugCommandResponsePayload response;
        zoneClient->sendDebugCommand(
            tbeq::net::DebugCommand::EquipItem,
            {grantItemBuffer},
            response);
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
