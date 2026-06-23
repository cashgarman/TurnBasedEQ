#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tbeq::test
{

class ProcessHandle
{
public:
    ProcessHandle() = default;
    ~ProcessHandle();

    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;

    ProcessHandle(ProcessHandle&& other) noexcept;
    ProcessHandle& operator=(ProcessHandle&& other) noexcept;

    bool valid() const;
    bool isRunning() const;
    void terminateProcess();

private:
    friend class TestCluster;
    explicit ProcessHandle(void* processHandle);

    void* processHandle_ = nullptr;
};

class TestCluster
{
public:
    explicit TestCluster(const std::filesystem::path& dbPath = {});
    ~TestCluster();

    uint16_t worldLoginPort() const { return worldLoginPort_; }
    uint16_t worldLoginClientPort() const { return worldLoginClientPort_; }
    const std::filesystem::path& dbPath() const { return dbPath_; }
    const std::filesystem::path& worldLoginExe() const { return worldLoginExe_; }
    const std::filesystem::path& zoneServerExe() const { return zoneServerExe_; }

    bool waitForWorldLoginReady(std::chrono::milliseconds timeout) const;
    bool waitForWorldLoginClientReady(std::chrono::milliseconds timeout) const;
    bool waitForTcpPort(uint16_t port, std::chrono::milliseconds timeout) const;
    bool startZoneServer(const std::string& zoneId, uint16_t clientPort, ProcessHandle& outProcess) const;

    static uint16_t pickEphemeralPort();
    static ProcessHandle spawnProcess(const std::vector<std::string>& args, const std::filesystem::path& workingDir);

    std::filesystem::path repoRoot_;
    std::filesystem::path worldLoginExe_;
    std::filesystem::path zoneServerExe_;
    std::filesystem::path dbPath_;
    std::unique_ptr<class TempDatabase> ownedDatabase_;
    uint16_t worldLoginPort_ = 0;
    uint16_t worldLoginClientPort_ = 0;
    ProcessHandle worldLoginProcess_;
};

} // namespace tbeq::test
