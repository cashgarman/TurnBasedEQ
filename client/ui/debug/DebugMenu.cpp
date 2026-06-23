#include "DebugMenu.hpp"

#include <imgui.h>

#include "LogViewer.hpp"
#include "UnitTestRunner.hpp"

namespace tbeq::ui
{

DebugMenu::DebugMenu(LogViewer& logViewer)
    : logViewer_(logViewer)
{
}

void DebugMenu::handleInput()
{
}

void DebugMenu::update(bool& visible)
{
    unitTestRunner_.update();

    if (!visible)
    {
        return;
    }

    if (ImGui::BeginTabBar("DebugTabs"))
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
            ImGui::TextUnformatted("Debug cheat panels will be added in later phases.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace tbeq::ui
