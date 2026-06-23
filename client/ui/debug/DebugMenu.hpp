#pragma once

#include <functional>
#include <string>

#include "UnitTestRunner.hpp"

namespace tbeq::client
{
class ZoneClient;
}

namespace tbeq::ui
{

class LogViewer;

class DebugMenu
{
public:
    explicit DebugMenu(LogViewer& logViewer);

    void update(
        tbeq::client::ZoneClient* zoneClient,
        const std::function<void(const std::string& line)>& appendLogLine);
    void handleInput();

private:
    void drawCheats(
        tbeq::client::ZoneClient* zoneClient,
        const std::function<void(const std::string& line)>& appendLogLine);

    LogViewer& logViewer_;
    UnitTestRunner unitTestRunner_;
};

} // namespace tbeq::ui
