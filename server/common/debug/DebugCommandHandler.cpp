#include "debug/DebugCommandHandler.hpp"

namespace tbeq::server
{

DebugCommandHandler::DebugCommandHandler(bool devModeEnabled)
    : devModeEnabled_(devModeEnabled)
{
}

DebugCommandResult DebugCommandHandler::handle(const net::DebugCommandRequestPayload& request) const
{
    if (!devModeEnabled_)
    {
        return {false, "Debug commands rejected: dev-mode is disabled"};
    }

    switch (request.command)
    {
    case net::DebugCommand::Ping:
        return {true, "pong"};
    case net::DebugCommand::SubscribeLogs:
        return {true, "log subscription stub"};
    case net::DebugCommand::ResetUILayout:
        return {true, "ui layout reset stub"};
    default:
        return {false, "Unknown debug command"};
    }
}

} // namespace tbeq::server
