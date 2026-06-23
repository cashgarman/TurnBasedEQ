#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

namespace tbeq::client
{

class LocalClusterLauncher
{
public:
    static constexpr uint16_t kWorldLoginPort = 9000;
    static constexpr uint16_t kWorldLoginClientPort = 9001;
    static constexpr uint16_t kZoneClientPort = 9100;

    static constexpr std::chrono::milliseconds kWorldLoginReadyTimeout{30000};
    static constexpr std::chrono::milliseconds kZoneReadyTimeout{15000};

    static std::filesystem::path detectRepoRoot();

    bool ensureRunning();
    void shutdownIfStarted();

private:
    bool isClusterReady() const;
    bool waitForClusterReady(std::chrono::milliseconds timeout) const;
    bool startWorldLogin(const std::filesystem::path& repoRoot);
    bool startZoneServer(const std::filesystem::path& repoRoot);
    bool isProcessRunning(unsigned long processId) const;

    bool startedWorldLogin_ = false;
    bool startedZoneServer_ = false;
#ifdef _WIN32
    unsigned long worldLoginPid_ = 0;
    unsigned long zoneServerPid_ = 0;
#endif
};

} // namespace tbeq::client
