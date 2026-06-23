#pragma once

#include <memory>
#include <string>
#include <vector>

namespace tbeq
{
class RingBufferSink;
}

namespace tbeq::ui
{

class LogViewer
{
public:
    LogViewer() = default;
    explicit LogViewer(std::shared_ptr<tbeq::RingBufferSink> sink);

    void setSink(std::shared_ptr<tbeq::RingBufferSink> sink) { sink_ = std::move(sink); }
    void draw();
    void clear();

private:
    std::shared_ptr<tbeq::RingBufferSink> sink_;
    bool autoScroll_ = true;
    bool paused_ = false;
    char filter_[128] = {};
    std::vector<std::string> cachedLines_;
};

} // namespace tbeq::ui
