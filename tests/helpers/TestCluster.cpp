#include "TestCluster.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <asio.hpp>
#include <windows.h>
#endif

namespace tbeq::test
{

namespace
{

std::filesystem::path findRepoRoot()
{
#ifdef TBEQ_REPO_ROOT
    return std::filesystem::path(TBEQ_REPO_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

std::string quoteArg(const std::string& arg)
{
    return "\"" + arg + "\"";
}

} // namespace

ProcessHandle::ProcessHandle(void* processHandle)
    : processHandle_(processHandle)
{
}

ProcessHandle::~ProcessHandle()
{
    terminateProcess();
}

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept
    : processHandle_(other.processHandle_)
{
    other.processHandle_ = nullptr;
}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept
{
    if (this != &other)
    {
        terminateProcess();
        processHandle_ = other.processHandle_;
        other.processHandle_ = nullptr;
    }
    return *this;
}

bool ProcessHandle::valid() const
{
#ifdef _WIN32
    return processHandle_ != nullptr;
#else
    return false;
#endif
}

void ProcessHandle::terminateProcess()
{
#ifdef _WIN32
    if (processHandle_ != nullptr)
    {
        auto* handle = static_cast<HANDLE>(processHandle_);
        TerminateProcess(handle, 0);
        WaitForSingleObject(handle, 5000);
        CloseHandle(handle);
        processHandle_ = nullptr;
    }
#endif
}

uint16_t TestCluster::pickEphemeralPort()
{
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    return acceptor.local_endpoint().port();
}

ProcessHandle TestCluster::spawnProcess(const std::vector<std::string>& args, const std::filesystem::path& workingDir)
{
#ifdef _WIN32
    std::ostringstream commandLine;
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            commandLine << ' ';
        }
        commandLine << quoteArg(args[i]);
    }

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::string command = commandLine.str();
    const BOOL created = CreateProcessA(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.string().c_str(),
        &startupInfo,
        &processInfo);

    if (!created)
    {
        return ProcessHandle{};
    }

    CloseHandle(processInfo.hThread);
    return ProcessHandle(processInfo.hProcess);
#else
    (void)args;
    (void)workingDir;
    return ProcessHandle{};
#endif
}

TestCluster::TestCluster()
    : repoRoot_(findRepoRoot())
    , worldLoginExe_(repoRoot_ / "build" / "server" / "Debug" / "tbeq_world_login.exe")
    , zoneServerExe_(repoRoot_ / "build" / "server" / "Debug" / "tbeq_zone_server.exe")
{
    if (!std::filesystem::exists(worldLoginExe_))
    {
        worldLoginExe_ = repoRoot_ / "build" / "server" / "Release" / "tbeq_world_login.exe";
        zoneServerExe_ = repoRoot_ / "build" / "server" / "Release" / "tbeq_zone_server.exe";
    }

    worldLoginPort_ = pickEphemeralPort();

    std::vector<std::string> args = {
        worldLoginExe_.string(),
        "--dev-mode",
        "--port",
        std::to_string(worldLoginPort_),
    };

    worldLoginProcess_ = spawnProcess(args, repoRoot_);
}

TestCluster::~TestCluster()
{
    worldLoginProcess_.terminateProcess();
}

bool TestCluster::waitForWorldLoginReady(std::chrono::milliseconds timeout) const
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        try
        {
            asio::io_context io;
            asio::ip::tcp::socket socket(io);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), worldLoginPort_);
            socket.connect(endpoint);
            return true;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    return false;
}

bool TestCluster::startZoneServer(const std::string& zoneId, uint16_t clientPort, ProcessHandle& outProcess) const
{
    std::vector<std::string> args = {
        zoneServerExe_.string(),
        "--dev-mode",
        "--zone-id",
        zoneId,
        "--world-login",
        "127.0.0.1",
        "--world-login-port",
        std::to_string(worldLoginPort_),
        "--client-port",
        std::to_string(clientPort),
    };

    outProcess = spawnProcess(args, repoRoot_);
    return outProcess.valid();
}

} // namespace tbeq::test
