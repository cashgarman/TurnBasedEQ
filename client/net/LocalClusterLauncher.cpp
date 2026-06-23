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
        spdlog::error("[cluster] CreateProcess failed (cmd={})", command);
        return false;
    }

    outProcessId = processInfo.dwProcessId;
    spdlog::info("[cluster] started PID {} with console: {}", outProcessId, args[0]);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
#endif

} // namespace

std::filesystem::path LocalClusterLauncher::detectRepoRoot()
{
    return findRepoRoot();
}

bool LocalClusterLauncher::isProcessRunning(unsigned long processId) const
{
#ifdef _WIN32
    if (processId == 0)
    {
        return false;
    }

    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr)
    {
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return waitResult == WAIT_TIMEOUT;
#else
    (void)processId;
    return false;
#endif
}

bool LocalClusterLauncher::isClusterReady() const
{
    return isTcpPortOpen(kWorldLoginClientPort) && isTcpPortOpen(kZoneClientPort);
}

bool LocalClusterLauncher::waitForClusterReady(std::chrono::milliseconds timeout) const
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (isClusterReady())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return isClusterReady();
}

bool LocalClusterLauncher::startWorldLogin(const std::filesystem::path& repoRoot)
{
#ifdef _WIN32
    const std::filesystem::path worldLoginExe = resolveServerExecutable(repoRoot, "tbeq_world_login");
    if (worldLoginExe.empty())
    {
        spdlog::error(
            "Server executables not found under {}. Build tbeq_world_login and tbeq_zone_server first.",
            (repoRoot / "build" / "server").string());
        return false;
    }

    const std::filesystem::path dbPath = repoRoot / kDbRelativePath;
    const std::filesystem::path dataRoot = repoRoot / "data";

    spdlog::info("[cluster] starting WorldLogin on port {} (client {})", kWorldLoginPort, kWorldLoginClientPort);

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
        spdlog::error("[cluster] failed to start tbeq_world_login");
        return false;
    }

    startedWorldLogin_ = true;

    if (!waitForTcpPort(kWorldLoginClientPort, kWorldLoginReadyTimeout))
    {
        if (!isProcessRunning(worldLoginPid_))
        {
            spdlog::error("[cluster] tbeq_world_login exited before becoming ready");
        }
        else
        {
            spdlog::error(
                "[cluster] WorldLogin client port {} did not become ready within {} ms",
                kWorldLoginClientPort,
                kWorldLoginReadyTimeout.count());
        }
        return false;
    }

    spdlog::info("[cluster] WorldLogin ready on client port {}", kWorldLoginClientPort);
    return true;
#else
    (void)repoRoot;
    return false;
#endif
}

bool LocalClusterLauncher::startZoneServer(const std::filesystem::path& repoRoot)
{
#ifdef _WIN32
    const std::filesystem::path zoneServerExe = resolveServerExecutable(repoRoot, "tbeq_zone_server");
    if (zoneServerExe.empty())
    {
        spdlog::error(
            "Server executables not found under {}. Build tbeq_world_login and tbeq_zone_server first.",
            (repoRoot / "build" / "server").string());
        return false;
    }

    const std::filesystem::path dbPath = repoRoot / kDbRelativePath;
    const std::filesystem::path dataRoot = repoRoot / "data";

    spdlog::info("[cluster] starting ZoneServer {} on client port {}", kZoneId, kZoneClientPort);

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

    if (!spawnServerProcessWithConsole(zoneServerArgs, repoRoot, zoneServerPid_))
    {
        spdlog::error("[cluster] failed to start tbeq_zone_server");
        return false;
    }

    startedZoneServer_ = true;

    if (!waitForTcpPort(kZoneClientPort, kZoneReadyTimeout))
    {
        if (!isProcessRunning(zoneServerPid_))
        {
            spdlog::error("[cluster] tbeq_zone_server exited before becoming ready");
        }
        else
        {
            spdlog::error(
                "[cluster] ZoneServer client port {} did not become ready within {} ms",
                kZoneClientPort,
                kZoneReadyTimeout.count());
        }
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    spdlog::info("[cluster] ZoneServer ready on client port {}", kZoneClientPort);
    return true;
#else
    (void)repoRoot;
    return false;
#endif
}

bool LocalClusterLauncher::ensureRunning()
{
    if (isClusterReady())
    {
        spdlog::info(
            "[cluster] local cluster already running (world-login client {}, zone {})",
            kWorldLoginClientPort,
            kZoneClientPort);
        return true;
    }

#ifndef _WIN32
    spdlog::error("[cluster] automatic local cluster startup is only supported on Windows for now");
    return false;
#else
    spdlog::info("[cluster] local cluster not ready; starting missing server processes");
    const std::filesystem::path repoRoot = findRepoRoot();
    spdlog::info("[cluster] repo root resolved to {}", repoRoot.string());

    if (!isTcpPortOpen(kWorldLoginClientPort))
    {
        if (!startWorldLogin(repoRoot))
        {
            return false;
        }
    }
    else
    {
        spdlog::info("[cluster] WorldLogin already listening on client port {}", kWorldLoginClientPort);
    }

    if (!isTcpPortOpen(kZoneClientPort))
    {
        if (!startZoneServer(repoRoot))
        {
            return false;
        }
    }
    else
    {
        spdlog::info("[cluster] ZoneServer already listening on client port {}", kZoneClientPort);
    }

    if (!waitForClusterReady(std::chrono::seconds(2)))
    {
        spdlog::error("[cluster] cluster ports did not become ready after startup");
        return false;
    }

    spdlog::info(
        "[cluster] local cluster ready (world-login client {}, zone {})",
        kWorldLoginClientPort,
        kZoneClientPort);
    return true;
#endif
}

void LocalClusterLauncher::shutdownIfStarted()
{
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

    if (startedZoneServer_)
    {
        terminatePid(zoneServerPid_);
        zoneServerPid_ = 0;
        startedZoneServer_ = false;
    }

    if (startedWorldLogin_)
    {
        terminatePid(worldLoginPid_);
        worldLoginPid_ = 0;
        startedWorldLogin_ = false;
    }
#endif
}

} // namespace tbeq::client
