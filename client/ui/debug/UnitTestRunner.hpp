#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tbeq::ui
{

class UnitTestRunner
{
public:
    UnitTestRunner();

    void update();
    void draw();

private:
    void resolveTestExecutable();
    void startRun();
    void clearOutput();
    void appendOutputLine(std::string line);

    std::filesystem::path testExePath_;
    std::vector<std::string> outputLines_;
    std::mutex outputMutex_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int> lastExitCode_{-1};
    int selectedPreset_ = 0;
    bool autoScroll_ = true;
    bool exeResolved_ = false;
};

} // namespace tbeq::ui
