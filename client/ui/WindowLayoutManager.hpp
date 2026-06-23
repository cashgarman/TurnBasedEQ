#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "GameWindow.hpp"

namespace tbeq::ui
{

class WindowLayoutManager
{
public:
    explicit WindowLayoutManager(std::filesystem::path layoutPath);

    void registerWindow(GameWindow& window);
    void load();
    void save() const;
    void resetToDefaults();
    void markDirty() { dirty_ = true; }

    static GameWindowState defaultState(const std::string& windowId);

private:
    static nlohmann::json stateToJson(const GameWindowState& state);
    static GameWindowState jsonToState(const nlohmann::json& json, const std::string& fallbackId);

    std::filesystem::path layoutPath_;
    std::unordered_map<std::string, GameWindow*> windows_;
    bool dirty_ = false;
};

} // namespace tbeq::ui
