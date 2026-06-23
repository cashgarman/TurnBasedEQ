#include "UnitTestRunner.hpp"

#include <algorithm>
#include <functional>
#include <sstream>

#include <imgui.h>
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

namespace tbeq::ui
{

namespace
{

struct TestPreset
{
    const char* label;
    const char* filter;
};

constexpr TestPreset kPresets[] =
{
    {"All unit tests", ""},
    {"Combat / spells", "[combat][spells]"},
    {"AI combat brain", "[ai]"},
    {"UI layout", "[ui]"},
    {"Auth", "[auth]"},
    {"Network packets", "[net]"},
    {"Server debug commands", "[server][debug]"},
    {"World generation", "[worldgen]"},
    {"World collision", "[world][collision]"},
    {"Render / tiles", "[render]"},
};

std::string quoteArg(const std::string& arg)
{
    return "\"" + arg + "\"";
}

#ifdef _WIN32
std::filesystem::path executableDirectory()
{
    char buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path findUnitTestExecutable()
{
#ifdef TBEQ_REPO_ROOT
    const std::filesystem::path repoRoot = TBEQ_REPO_ROOT;
    const std::filesystem::path candidates[] =
    {
        repoRoot / "build" / "tests" / "Debug" / "tbeq_tests_unit.exe",
        repoRoot / "build" / "tests" / "Release" / "tbeq_tests_unit.exe",
    };
    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
#endif

    const std::filesystem::path clientDir = executableDirectory();
    if (clientDir.empty())
    {
        return {};
    }

    const std::filesystem::path relativeCandidates[] =
    {
        clientDir / ".." / ".." / "tests" / "Debug" / "tbeq_tests_unit.exe",
        clientDir / ".." / ".." / "tests" / "Release" / "tbeq_tests_unit.exe",
    };
    for (const auto& candidate : relativeCandidates)
    {
        const auto resolved = std::filesystem::weakly_canonical(candidate);
        if (std::filesystem::exists(resolved))
        {
            return resolved;
        }
    }

    return {};
}

bool runProcessCaptureOutput(
    const std::filesystem::path& executable,
    const std::string& filter,
    const std::function<void(std::string)>& onLine,
    int& exitCode)
{
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readHandle = nullptr;
    HANDLE writeHandle = nullptr;
    if (!CreatePipe(&readHandle, &writeHandle, &securityAttributes, 0))
    {
        return false;
    }

    SetHandleInformation(readHandle, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = writeHandle;
    startupInfo.hStdError = writeHandle;

    PROCESS_INFORMATION processInfo{};

    std::ostringstream commandLine;
    commandLine << quoteArg(executable.string());
    if (!filter.empty())
    {
        commandLine << ' ' << quoteArg(filter);
    }

    const std::string command = commandLine.str();

    const std::filesystem::path workingDirectory =
#ifdef TBEQ_REPO_ROOT
        std::filesystem::path(TBEQ_REPO_ROOT);
#else
        executable.parent_path();
#endif
    const std::string workingDirectoryStr = workingDirectory.string();

    // CreateProcess may modify the command line buffer in place.
    std::string mutableCommand = command;
    const BOOL created = CreateProcessA(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDirectoryStr.c_str(),
        &startupInfo,
        &processInfo);

    CloseHandle(writeHandle);

    if (!created)
    {
        CloseHandle(readHandle);
        return false;
    }

    char buffer[512]{};
    DWORD bytesRead = 0;
    std::string pending;
    while (ReadFile(readHandle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        pending.append(buffer, bytesRead);
        std::size_t start = 0;
        for (std::size_t i = 0; i < pending.size(); ++i)
        {
            if (pending[i] == '\n')
            {
                std::string line = pending.substr(start, i - start);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                onLine(std::move(line));
                start = i + 1;
            }
        }
        pending.erase(0, start);
    }

    if (!pending.empty())
    {
        onLine(std::move(pending));
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD processExitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &processExitCode);
    exitCode = static_cast<int>(processExitCode);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    CloseHandle(readHandle);
    return true;
}
#endif

} // namespace

UnitTestRunner::UnitTestRunner()
{
    resolveTestExecutable();
}

void UnitTestRunner::resolveTestExecutable()
{
    testExePath_ = findUnitTestExecutable();
    exeResolved_ = !testExePath_.empty();
    if (!exeResolved_)
    {
        spdlog::warn("Unit test runner could not locate tbeq_tests_unit.exe");
    }
}

void UnitTestRunner::clearOutput()
{
    std::lock_guard lock(outputMutex_);
    outputLines_.clear();
    lastExitCode_.store(-1);
}

void UnitTestRunner::appendOutputLine(std::string line)
{
    std::lock_guard lock(outputMutex_);
    outputLines_.push_back(std::move(line));
}

void UnitTestRunner::startRun()
{
    if (running_.load() || !exeResolved_)
    {
        return;
    }

    if (worker_.joinable())
    {
        worker_.join();
    }

    clearOutput();
    running_.store(true);
    lastExitCode_.store(-1);

    const std::filesystem::path executable = testExePath_;
    const std::string filter = kPresets[selectedPreset_].filter;

    worker_ = std::thread(
        [this, executable, filter]()
        {
#ifdef _WIN32
            appendOutputLine("Running: " + executable.string() + (filter.empty() ? "" : " " + filter));

            int exitCode = 1;
            const bool ok = runProcessCaptureOutput(
                executable,
                filter,
                [this](std::string line) { appendOutputLine(std::move(line)); },
                exitCode);

            if (!ok)
            {
                appendOutputLine("Failed to launch unit test executable.");
                lastExitCode_.store(1);
            }
            else
            {
                lastExitCode_.store(exitCode);
                appendOutputLine(exitCode == 0 ? "All tests passed." : "Tests failed.");
            }
#else
            appendOutputLine("Unit test runner is only supported on Windows.");
            lastExitCode_.store(1);
#endif
            running_.store(false);
        });
}

void UnitTestRunner::update()
{
    if (!worker_.joinable() || running_.load())
    {
        return;
    }

    worker_.join();
}

void UnitTestRunner::draw()
{
    update();

    if (!exeResolved_)
    {
        ImGui::TextWrapped(
            "Could not find tbeq_tests_unit.exe. Build tests with -DTBEQ_BUILD_TESTS=ON, "
            "then run from the repo root so the runner can locate build/tests/Debug/tbeq_tests_unit.exe.");
        if (ImGui::Button("Retry locate"))
        {
            resolveTestExecutable();
        }
        return;
    }

    const std::string testExePathDisplay = testExePath_.string();
    ImGui::TextDisabled("%s", testExePathDisplay.c_str());

    const int presetCount = static_cast<int>(std::size(kPresets));
    if (ImGui::Combo("Test suite", &selectedPreset_, [](void* data, int index)
        {
            const auto* presets = static_cast<const TestPreset*>(data);
            return presets[index].label;
        }, const_cast<TestPreset*>(kPresets), presetCount))
    {
    }

    ImGui::SameLine();
    const bool canRun = !running_.load();
    if (!canRun)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Run"))
    {
        startRun();
    }
    if (!canRun)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        clearOutput();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    if (lastExitCode_.load() >= 0)
    {
        const int exitCode = lastExitCode_.load();
        if (exitCode == 0)
        {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Exit code: %d", exitCode);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Exit code: %d", exitCode);
        }
    }
    else if (running_.load())
    {
        ImGui::TextDisabled("Running...");
    }

    std::vector<std::string> lines;
    {
        std::lock_guard lock(outputMutex_);
        lines = outputLines_;
    }

    ImGui::BeginChild("UnitTestOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (lines.empty())
    {
        ImGui::TextDisabled("Choose a suite and press Run. Catch2 output appears here.");
    }
    for (const auto& line : lines)
    {
        ImGui::TextUnformatted(line.c_str());
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

} // namespace tbeq::ui
