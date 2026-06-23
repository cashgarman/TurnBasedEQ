#include "WindowLayoutManager.hpp"

#include <fstream>

#include <spdlog/spdlog.h>

namespace tbeq::ui
{

WindowLayoutManager::WindowLayoutManager(std::filesystem::path layoutPath)
    : layoutPath_(std::move(layoutPath))
{
}

void WindowLayoutManager::registerWindow(GameWindow& window)
{
    windows_[window.id()] = &window;
}

GameWindowState WindowLayoutManager::defaultState(const std::string& windowId)
{
    GameWindowState state;
    state.id = windowId;
    state.visible = true;

    if (windowId == "hud")
    {
        state.title = "HUD";
        state.x = 16.0f;
        state.y = 16.0f;
        state.width = 240.0f;
        state.height = 80.0f;
    }
    else if (windowId == "chat")
    {
        state.title = "Chat";
        state.x = 16.0f;
        state.y = 480.0f;
        state.width = 360.0f;
        state.height = 200.0f;
    }
    else if (windowId == "debug_menu")
    {
        state.title = "Debug Menu";
        state.x = 420.0f;
        state.y = 80.0f;
        state.width = 520.0f;
        state.height = 420.0f;
        state.visible = false;
    }
    else
    {
        state.title = windowId;
        state.x = 100.0f;
        state.y = 100.0f;
        state.width = 320.0f;
        state.height = 240.0f;
    }

    return state;
}

nlohmann::json WindowLayoutManager::stateToJson(const GameWindowState& state)
{
    return nlohmann::json{
        {"id", state.id},
        {"title", state.title},
        {"x", state.x},
        {"y", state.y},
        {"width", state.width},
        {"height", state.height},
        {"visible", state.visible},
        {"pinned", state.pinned},
    };
}

GameWindowState WindowLayoutManager::jsonToState(const nlohmann::json& json, const std::string& fallbackId)
{
    GameWindowState state = defaultState(fallbackId);
    state.id = json.value("id", fallbackId);
    state.title = json.value("title", state.title);
    state.x = json.value("x", state.x);
    state.y = json.value("y", state.y);
    state.width = json.value("width", state.width);
    state.height = json.value("height", state.height);
    state.visible = json.value("visible", state.visible);
    state.pinned = json.value("pinned", state.pinned);
    return state;
}

void WindowLayoutManager::load()
{
    if (!std::filesystem::exists(layoutPath_))
    {
        spdlog::info("No UI layout file at {}; using defaults", layoutPath_.string());
        return;
    }

    std::ifstream input(layoutPath_);
    nlohmann::json root;
    input >> root;

    if (!root.contains("windows") || !root["windows"].is_array())
    {
        spdlog::warn("UI layout file missing windows array");
        return;
    }

    for (const auto& entry : root["windows"])
    {
        const std::string id = entry.value("id", std::string{});
        auto it = windows_.find(id);
        if (it == windows_.end())
        {
            continue;
        }
        it->second->applyState(jsonToState(entry, id));
    }
}

void WindowLayoutManager::save() const
{
    std::filesystem::create_directories(layoutPath_.parent_path());

    nlohmann::json root;
    root["version"] = 1;
    root["windows"] = nlohmann::json::array();

    for (const auto& [id, window] : windows_)
    {
        (void)id;
        root["windows"].push_back(stateToJson(window->state()));
    }

    std::ofstream output(layoutPath_);
    output << root.dump(2);
}

void WindowLayoutManager::resetToDefaults()
{
    for (auto& [id, window] : windows_)
    {
        window->applyState(defaultState(id));
    }
    dirty_ = true;
}

} // namespace tbeq::ui
