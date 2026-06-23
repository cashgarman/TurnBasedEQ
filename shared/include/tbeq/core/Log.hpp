#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

namespace tbeq
{

class RingBufferSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit RingBufferSink(std::size_t maxLines = 8192);

    std::vector<std::string> snapshot() const;

    void clear();

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    mutable std::mutex mutex_;
    std::deque<std::string> lines_;
    std::size_t maxLines_;
};

void initLogging(const std::string& processName);
std::shared_ptr<spdlog::logger> getLogger();
std::shared_ptr<RingBufferSink> getRingBufferSink();

} // namespace tbeq
