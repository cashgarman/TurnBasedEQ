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

    using TeleportToZoneCallback = std::function<bool(const std::string& zoneId, std::string& outMessage)>;

    void update(
        tbeq::client::ZoneClient* zoneClient,
        const std::function<void(const std::string& line)>& appendLogLine,
        const TeleportToZoneCallback& teleportToZone = {});
    void handleInput();

private:
    void drawCheats(
        tbeq::client::ZoneClient* zoneClient,
        const std::function<void(const std::string& line)>& appendLogLine,
        const TeleportToZoneCallback& teleportToZone);

    LogViewer& logViewer_;
    UnitTestRunner unitTestRunner_;
};

} // namespace tbeq::ui
