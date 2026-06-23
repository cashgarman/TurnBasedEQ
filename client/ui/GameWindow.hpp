#pragma once

#include <string>

namespace tbeq::ui
{

struct GameWindowState
{
    std::string id;
    std::string title;
    float x = 100.0f;
    float y = 100.0f;
    float width = 320.0f;
    float height = 240.0f;
    bool visible = true;
    bool pinned = false;
};

class GameWindow
{
public:
    GameWindow(std::string id, std::string title, float minWidth, float minHeight);

    bool begin(int displayWidth, int displayHeight);
    void end();

    const std::string& id() const { return state_.id; }
    GameWindowState& state() { return state_; }
    const GameWindowState& state() const { return state_; }

    void applyState(const GameWindowState& state);
    void clampToDisplay(int displayWidth, int displayHeight);
    bool syncFromImGui();

    GameWindowState state_;
    float minWidth_;
    float minHeight_;
    int displayWidth_ = 0;
    int displayHeight_ = 0;
    bool open_ = true;
    bool pendingLayoutApply_ = true;
    bool firstUseApplied_ = false;
    bool imguiBegun_ = false;
};

// Call immediately after ImGui::NewFrame() to clamp active title-bar drags before shell windows begin.
void constrainActiveImGuiWindowDrag(int displayWidth, int displayHeight);

} // namespace tbeq::ui
