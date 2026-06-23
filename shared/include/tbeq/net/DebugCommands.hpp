#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq::net
{

enum class DebugCommand : uint16_t
{
    Ping = 1,
    SubscribeLogs = 2,
    ResetUILayout = 3,
    Unknown = 9999,
};

struct DebugCommandRequestPayload
{
    DebugCommand command = DebugCommand::Unknown;
    std::vector<std::string> args;
};

struct DebugCommandResponsePayload
{
    bool ok = false;
    std::string message;
};

} // namespace tbeq::net
