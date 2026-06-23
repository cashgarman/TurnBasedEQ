#include "DebugMenu.hpp"

#include <imgui.h>
#include <sstream>
#include <utility>
#include <vector>

#include "LogViewer.hpp"
#include "UnitTestRunner.hpp"
#include "net/ZoneClient.hpp"
#include "tbeq/net/DebugCommands.hpp"

namespace tbeq::ui
{

namespace
{

struct ZoneOption
{
    std::string id;
    std::string label;
};

std::vector<ZoneOption> parseZoneListMessage(const std::string& message)
{
    std::vector<ZoneOption> options;
    std::stringstream stream(message);
    std::string entry;
    while (std::getline(stream, entry, ';'))
    {
        if (entry.empty())
        {
            continue;
        }

        const auto separator = entry.find(':');
        if (separator == std::string::npos)
        {
            options.push_back(ZoneOption{entry, entry});
            continue;
        }

        ZoneOption option;
        option.id = entry.substr(0, separator);
        option.label = entry.substr(separator + 1);
        if (option.label.empty())
        {
            option.label = option.id;
        }
        options.push_back(std::move(option));
    }
    return options;
}

} // namespace

DebugMenu::DebugMenu(LogViewer& logViewer)
    : logViewer_(logViewer)
{
}

void DebugMenu::handleInput()
{
}

void DebugMenu::drawCheats(
    tbeq::client::ZoneClient* zoneClient,
    const std::function<void(const std::string& line)>& appendLogLine,
    const TeleportToZoneCallback& teleportToZone)
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

    ImGui::Separator();
    ImGui::TextUnformatted("Travel cheats");
    static std::vector<ZoneOption> zoneOptions;
    static int selectedZoneIndex = 0;
    static bool zonesLoaded = false;
    static bool zonesLoadFailed = false;

    if (!zonesLoaded && !zonesLoadFailed)
    {
        tbeq::net::DebugCommandResponsePayload response;
        if (zoneClient->sendDebugCommand(tbeq::net::DebugCommand::ListZones, {}, response) && response.ok)
        {
            zoneOptions = parseZoneListMessage(response.message);
            zonesLoaded = true;
            selectedZoneIndex = 0;
        }
        else
        {
            zonesLoadFailed = true;
            appendLogLine("[Debug] Failed to load zone list.");
        }
    }

    if (zonesLoaded && !zoneOptions.empty())
    {
        const char* preview = zoneOptions[static_cast<std::size_t>(selectedZoneIndex)].label.c_str();
        if (ImGui::BeginCombo("Zone", preview))
        {
            for (int i = 0; i < static_cast<int>(zoneOptions.size()); ++i)
            {
                const bool selected = selectedZoneIndex == i;
                if (ImGui::Selectable(zoneOptions[static_cast<std::size_t>(i)].label.c_str(), selected))
                {
                    selectedZoneIndex = i;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Teleport to zone center"))
        {
            if (teleportToZone)
            {
                std::string message;
                const auto& zone = zoneOptions[static_cast<std::size_t>(selectedZoneIndex)];
                if (teleportToZone(zone.id, message))
                {
                    appendLogLine("[Debug] " + message);
                }
                else
                {
                    appendLogLine("[Debug] " + (message.empty() ? "Teleport failed." : message));
                }
            }
            else
            {
                appendLogLine("[Debug] Teleport handler unavailable.");
            }
        }
    }
    else if (zonesLoadFailed)
    {
        ImGui::TextUnformatted("Could not load zone list.");
        if (ImGui::Button("Retry zone list"))
        {
            zonesLoadFailed = false;
        }
    }
    else if (zonesLoaded)
    {
        ImGui::TextUnformatted("No zones available.");
    }
    else
    {
        ImGui::TextUnformatted("Loading zones...");
    }
}

void DebugMenu::update(
    tbeq::client::ZoneClient* zoneClient,
    const std::function<void(const std::string& line)>& appendLogLine,
    const TeleportToZoneCallback& teleportToZone)
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
            drawCheats(zoneClient, appendLogLine, teleportToZone);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace tbeq::ui
