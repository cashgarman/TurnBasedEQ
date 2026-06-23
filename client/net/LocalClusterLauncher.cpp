#include "net/LocalClusterLauncher.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tbeq::client
{

namespace
{

constexpr const char* kZoneId = "starter_city";
constexpr const char* kDbRelativePath = "config/tbeq.db";

std::filesystem::path findRepoRoot()
{
#ifdef TBEQ_REPO_ROOT
    const std::filesystem::path configuredRoot = TBEQ_REPO_ROOT;
    if (std::filesystem::exists(configuredRoot / "data" / "tile_defs.json"))
    {
        return configuredRoot;
    }
#endif

    std::filesystem::path candidate = std::filesystem::current_path();
    for (int depth = 0; depth < 6; ++depth)
    {
        if (std::filesystem::exists(candidate / "data" / "tile_defs.json"))
        {
            return candidate;
        }

        if (!candidate.has_parent_path())
        {
            break;
        }
        candidate = candidate.parent_path();
    }

#ifdef _WIN32
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        candidate = std::filesystem::path(modulePath).parent_path();
        for (int depth = 0; depth < 8; ++depth)
        {
            if (std::filesystem::exists(candidate / "data" / "tile_defs.json"))
            {
                return candidate;
            }

            if (!candidate.has_parent_path())
            {
                break;
            }
            candidate = candidate.parent_path();
        }
    }
#endif

    return std::filesystem::current_path();
}

std::filesystem::path resolveServerExecutable(const std::filesystem::path& repoRoot, const char* executableName)
{
    const std::filesystem::path debugExe =
        repoRoot / "build" / "server" / "Debug" / (std::string(executableName) + ".exe");
    if (std::filesystem::exists(debugExe))
    {
        return debugExe;
    }

    const std::filesystem::path releaseExe =
        repoRoot / "build" / "server" / "Release" / (std::string(executableName) + ".exe");
    if (std::filesystem::exists(releaseExe))
    {
        return releaseExe;
    }

    return {};
}

bool waitForTcpPort(uint16_t port, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        try
        {
            asio::io_context io;
            asio::ip::tcp::socket socket(io);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
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

bool isTcpPortOpen(uint16_t port)
{
    return waitForTcpPort(port, std::chrono::milliseconds(250));
}

std::string quoteArg(const std::string& arg)
{
    return "\"" + arg + "\"";
}

#ifdef _WIN32
bool spawnServerProcessWithConsole(
    const std::vector<std::string>& args,
    const std::filesystem::path& workingDir,
    unsigned long& outProcessId)
{
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
        CREATE_NEW_CONSOLE,
        nullptr,
        workingDir.string().c_str(),
        &startupInfo,
        &processInfo);

    if (!created)
    {
        spdlog::error("[login] CreateProcess failed (cmd={})", command);
        return false;
    }

    outProcessId = processInfo.dwProcessId;
    spdlog::info("[login] started PID {} with console: {}", outProcessId, args[0]);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
#endif

} // namespace

bool LocalClusterLauncher::ensureRunning()
{
    if (isTcpPortOpen(kWorldLoginClientPort))
    {
        spdlog::info("[login] local cluster already running on client port {}", kWorldLoginClientPort);
        return true;
    }

#ifdef _WIN32
    spdlog::info("[login] local cluster not detected; starting servers");
    const std::filesystem::path repoRoot = findRepoRoot();
    spdlog::info("[login] repo root resolved to {}", repoRoot.string());
    const std::filesystem::path worldLoginExe = resolveServerExecutable(repoRoot, "tbeq_world_login");
    const std::filesystem::path zoneServerExe = resolveServerExecutable(repoRoot, "tbeq_zone_server");
    if (worldLoginExe.empty() || zoneServerExe.empty())
    {
        spdlog::error(
            "Server executables not found under {}. Build tbeq_world_login and tbeq_zone_server first.",
            (repoRoot / "build" / "server").string());
        return false;
    }

    const std::filesystem::path dbPath = repoRoot / kDbRelativePath;
    const std::filesystem::path dataRoot = repoRoot / "data";

    spdlog::info("[login] starting WorldLogin on port {} (client {})", kWorldLoginPort, kWorldLoginClientPort);

    const std::vector<std::string> worldLoginArgs = {
        worldLoginExe.string(),
        "--dev-mode",
        "--port",
        std::to_string(kWorldLoginPort),
        "--world-login-client-port",
        std::to_string(kWorldLoginClientPort),
        "--db-path",
        dbPath.string(),
        "--data-root",
        dataRoot.string(),
    };

    if (!spawnServerProcessWithConsole(worldLoginArgs, repoRoot, worldLoginPid_))
    {
        spdlog::error("Failed to start tbeq_world_login");
        return false;
    }

    if (!waitForTcpPort(kWorldLoginPort, std::chrono::seconds(8)))
    {
        spdlog::error("WorldLogin did not become ready on port {}", kWorldLoginPort);
        return false;
    }

    spdlog::info("[login] WorldLogin ready on port {}", kWorldLoginPort);

    const std::vector<std::string> zoneServerArgs = {
        zoneServerExe.string(),
        "--dev-mode",
        "--zone-id",
        kZoneId,
        "--world-login",
        "127.0.0.1",
        "--world-login-port",
        std::to_string(kWorldLoginPort),
        "--client-port",
        std::to_string(kZoneClientPort),
        "--db-path",
        dbPath.string(),
        "--data-root",
        dataRoot.string(),
    };

    spdlog::info("[login] starting ZoneServer {} on client port {}", kZoneId, kZoneClientPort);
    if (!spawnServerProcessWithConsole(zoneServerArgs, repoRoot, zoneServerPid_))
    {
        spdlog::error("Failed to start tbeq_zone_server");
        return false;
    }

    if (!waitForTcpPort(kZoneClientPort, std::chrono::seconds(8)))
    {
        spdlog::error("ZoneServer did not become ready on client port {}", kZoneClientPort);
        return false;
    }

    spdlog::info("[login] ZoneServer ready on client port {}", kZoneClientPort);

    startedByClient_ = true;
    spdlog::info(
        "[login] local cluster started (world-login {} client {}, zone {})",
        kWorldLoginPort,
        kWorldLoginClientPort,
        kZoneClientPort);
    return true;
#else
    spdlog::error("Automatic local cluster startup is only supported on Windows for now");
    return false;
#endif
}

void LocalClusterLauncher::shutdownIfStarted()
{
    if (!startedByClient_)
    {
        return;
    }

#ifdef _WIN32
    auto terminatePid = [](unsigned long processId)
    {
        if (processId == 0)
        {
            return;
        }

        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
        if (process != nullptr)
        {
            TerminateProcess(process, 0);
            CloseHandle(process);
        }
    };

    terminatePid(zoneServerPid_);
    terminatePid(worldLoginPid_);
    worldLoginPid_ = 0;
    zoneServerPid_ = 0;
#endif

    startedByClient_ = false;
}

} // namespace tbeq::client
