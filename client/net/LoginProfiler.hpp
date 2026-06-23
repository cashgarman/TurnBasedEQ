#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace tbeq::client
{

class LoginProfiler
{
public:
    class Scope
    {
    public:
        Scope(LoginProfiler& profiler, const std::string& step);
        ~Scope();

    private:
        LoginProfiler& profiler_;
        std::string step_;
        std::chrono::steady_clock::time_point start_;
    };

    LoginProfiler();

    std::string summary() const;
    void logSummary(const char* title) const;

private:
    void record(const std::string& step, double durationMs);

    std::chrono::steady_clock::time_point flowStart_;
    std::vector<std::pair<std::string, double>> steps_;
};

} // namespace tbeq::client
