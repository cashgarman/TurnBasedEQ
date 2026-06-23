#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
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
    void terminateProcess();

private:
    friend class TestCluster;
    explicit ProcessHandle(void* processHandle);

    void* processHandle_ = nullptr;
};

class TestCluster
{
public:
    TestCluster();
    ~TestCluster();

    uint16_t worldLoginPort() const { return worldLoginPort_; }
    const std::filesystem::path& worldLoginExe() const { return worldLoginExe_; }
    const std::filesystem::path& zoneServerExe() const { return zoneServerExe_; }

    bool waitForWorldLoginReady(std::chrono::milliseconds timeout) const;
    bool startZoneServer(const std::string& zoneId, uint16_t clientPort, ProcessHandle& outProcess) const;

private:
    static uint16_t pickEphemeralPort();
    static ProcessHandle spawnProcess(const std::vector<std::string>& args, const std::filesystem::path& workingDir);

    std::filesystem::path repoRoot_;
    std::filesystem::path worldLoginExe_;
    std::filesystem::path zoneServerExe_;
    uint16_t worldLoginPort_ = 0;
    ProcessHandle worldLoginProcess_;
};

} // namespace tbeq::test
