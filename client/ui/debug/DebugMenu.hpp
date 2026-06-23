#pragma once

#include "UnitTestRunner.hpp"

namespace tbeq::ui
{

class LogViewer;

class DebugMenu
{
public:
    explicit DebugMenu(LogViewer& logViewer);

    void update(bool& visible);
    void handleInput();

private:
    LogViewer& logViewer_;
    UnitTestRunner unitTestRunner_;
};

} // namespace tbeq::ui
