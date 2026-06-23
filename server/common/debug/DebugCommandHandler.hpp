#pragma once

#include <string>
#include <vector>

#include "tbeq/net/DebugCommands.hpp"

namespace tbeq::server
{

struct DebugCommandResult
{
    bool ok = false;
    std::string message;
};

class DebugCommandHandler
{
public:
    explicit DebugCommandHandler(bool devModeEnabled);

    DebugCommandResult handle(const net::DebugCommandRequestPayload& request) const;
    bool devModeEnabled() const { return devModeEnabled_; }

private:
    bool devModeEnabled_;
};

} // namespace tbeq::server
