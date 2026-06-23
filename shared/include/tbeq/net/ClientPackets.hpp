#pragma once

#include <cstdint>
#include <string>

#include "tbeq/net/DebugCommands.hpp"
#include "tbeq/net/PacketTypes.hpp"

namespace tbeq::net
{

struct ClientPingPayload
{
    uint64_t timestampMs = 0;
};

struct ClientPongPayload
{
    uint64_t timestampMs = 0;
};

struct ZoneRegisterPayload
{
    std::string zoneId;
    std::string listenHost;
    uint16_t listenPort = 0;
    uint32_t capacity = 100;
    std::string version;
};

struct ZoneRegisterAckPayload
{
    bool accepted = false;
    std::string message;
};

using ClientDebugCommandRequestPayload = DebugCommandRequestPayload;
using ClientDebugCommandResponsePayload = DebugCommandResponsePayload;

} // namespace tbeq::net
