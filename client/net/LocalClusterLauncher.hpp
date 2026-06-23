#pragma once

#include <array>
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

    static constexpr uint16_t kStarterCityPort = 9100;
    static constexpr uint16_t kStarterHuntingPort = 9101;
    static constexpr uint16_t kStarterDungeonPort = 9102;

    static constexpr std::chrono::milliseconds kWorldLoginReadyTimeout{30000};
    static constexpr std::chrono::milliseconds kZoneReadyTimeout{15000};

    static std::filesystem::path detectRepoRoot();

    bool ensureRunning();
    void shutdownCluster();

private:
    struct ZoneEndpoint
    {
        const char* zoneId;
        uint16_t clientPort;
    };

    static constexpr std::array<ZoneEndpoint, 3> kZoneEndpoints{{
        {"starter_city", kStarterCityPort},
        {"starter_hunting", kStarterHuntingPort},
        {"starter_dungeon", kStarterDungeonPort},
    }};

    struct StartedZoneServer
    {
        bool started = false;
        unsigned long pid = 0;
    };

    bool isClusterReady() const;
    bool waitForClusterReady(std::chrono::milliseconds timeout) const;
    bool startWorldLogin(const std::filesystem::path& repoRoot);
    bool startZoneServer(const std::filesystem::path& repoRoot, const char* zoneId, uint16_t clientPort, StartedZoneServer& outStarted);
    bool isProcessRunning(unsigned long processId) const;
    void terminateTrackedProcesses();
    void terminateClusterServerProcesses();

    bool startedWorldLogin_ = false;
    std::array<StartedZoneServer, kZoneEndpoints.size()> startedZoneServers_{};
#ifdef _WIN32
    unsigned long worldLoginPid_ = 0;
#endif
};

} // namespace tbeq::client
