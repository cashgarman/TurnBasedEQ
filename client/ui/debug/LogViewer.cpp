#include "LogViewer.hpp"

#include <algorithm>

#include <imgui.h>

#include "tbeq/core/Log.hpp"

namespace tbeq::ui
{

LogViewer::LogViewer(std::shared_ptr<tbeq::RingBufferSink> sink)
    : sink_(std::move(sink))
{
}

void LogViewer::clear()
{
    if (sink_)
    {
        sink_->clear();
    }
    cachedLines_.clear();
}

void LogViewer::draw()
{
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    ImGui::Checkbox("Pause", &paused_);
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        clear();
    }

    ImGui::InputText("Filter", filter_, sizeof(filter_));

    if (!paused_ && sink_)
    {
        cachedLines_ = sink_->snapshot();
    }

    const std::string filterText = filter_;
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (cachedLines_.empty())
    {
        ImGui::TextDisabled("No log entries yet. Startup messages appear here after launch.");
    }
    for (const auto& line : cachedLines_)
    {
        if (!filterText.empty() && line.find(filterText) == std::string::npos)
        {
            continue;
        }
        ImGui::TextUnformatted(line.c_str());
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

} // namespace tbeq::ui
