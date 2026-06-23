#pragma once

#include <cstdint>

namespace tbeq::client
{

class LocalClusterLauncher
{
public:
    static constexpr uint16_t kWorldLoginPort = 9000;
    static constexpr uint16_t kWorldLoginClientPort = 9001;
    static constexpr uint16_t kZoneClientPort = 9100;

    bool ensureRunning();
    void shutdownIfStarted();

private:
    bool startedByClient_ = false;
#ifdef _WIN32
    unsigned long worldLoginPid_ = 0;
    unsigned long zoneServerPid_ = 0;
#endif
};

} // namespace tbeq::client
