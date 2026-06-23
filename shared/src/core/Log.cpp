#include "tbeq/core/Log.hpp"

#include <deque>
#include <mutex>
#include <vector>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tbeq
{

namespace
{
std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<RingBufferSink> g_ringSink;
} // namespace

RingBufferSink::RingBufferSink(std::size_t maxLines)
    : maxLines_(maxLines)
{
}

std::vector<std::string> RingBufferSink::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {lines_.begin(), lines_.end()};
}

void RingBufferSink::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.clear();
}

void RingBufferSink::sink_it_(const spdlog::details::log_msg& msg)
{
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string line(formatted.data(), formatted.size());
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    {
        line.pop_back();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(std::move(line));
    while (lines_.size() > maxLines_)
    {
        lines_.pop_front();
    }
}

void RingBufferSink::flush_()
{
}

void initLogging(const std::string& processName)
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    g_ringSink = std::make_shared<RingBufferSink>();

    g_logger = std::make_shared<spdlog::logger>(
        processName,
        spdlog::sinks_init_list{consoleSink, g_ringSink});
    g_logger->set_level(spdlog::level::trace);
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    spdlog::set_default_logger(g_logger);
}

std::shared_ptr<spdlog::logger> getLogger()
{
    return g_logger;
}

std::shared_ptr<RingBufferSink> getRingBufferSink()
{
    return g_ringSink;
}

} // namespace tbeq
