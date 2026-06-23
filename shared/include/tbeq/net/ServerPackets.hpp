#pragma once

#include <cstdint>
#include <string>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/DebugCommands.hpp"
#include "tbeq/net/PacketTypes.hpp"

namespace tbeq::net
{

struct ServerPingPayload
{
    uint64_t timestampMs = 0;
};

struct ServerPongPayload
{
    uint64_t timestampMs = 0;
};

using ServerDebugCommandRequestPayload = DebugCommandRequestPayload;
using ServerDebugCommandResponsePayload = DebugCommandResponsePayload;

} // namespace tbeq::net
