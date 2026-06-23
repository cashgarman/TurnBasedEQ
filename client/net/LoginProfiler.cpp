#include "net/LoginProfiler.hpp"

#include <sstream>

#include <spdlog/spdlog.h>

namespace tbeq::client
{

LoginProfiler::LoginProfiler()
    : flowStart_(std::chrono::steady_clock::now())
{
}

LoginProfiler::Scope::Scope(LoginProfiler& profiler, const std::string& step)
    : profiler_(profiler)
    , step_(step)
    , start_(std::chrono::steady_clock::now())
{
    spdlog::info("[login] begin {}", step_);
}

LoginProfiler::Scope::~Scope()
{
    const auto end = std::chrono::steady_clock::now();
    const double durationMs =
        std::chrono::duration<double, std::milli>(end - start_).count();
    profiler_.record(step_, durationMs);
    spdlog::info("[login] end {} ({:.1f} ms)", step_, durationMs);
}

void LoginProfiler::record(const std::string& step, double durationMs)
{
    steps_.emplace_back(step, durationMs);
}

std::string LoginProfiler::summary() const
{
    const auto end = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(end - flowStart_).count();

    std::ostringstream stream;
    stream << "total=" << totalMs << " ms";
    for (const auto& [step, durationMs] : steps_)
    {
        stream << "\n  " << step << ": " << durationMs << " ms";
    }
    return stream.str();
}

void LoginProfiler::logSummary(const char* title) const
{
    spdlog::info("[login] {} profile:\n{}", title, summary());
}

} // namespace tbeq::client
