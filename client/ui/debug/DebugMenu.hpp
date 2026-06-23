#pragma once

namespace tbeq::ui
{

class DebugMenu
{
public:
    explicit DebugMenu(class LogViewer& logViewer);

    void update(bool& visible);
    void handleInput();

private:
    LogViewer& logViewer_;
    bool showLogPanel_ = true;
};

} // namespace tbeq::ui
