#pragma once

#include <cstdint>

namespace tbeq::net
{

enum class ClientPacketType : uint16_t
{
    Ping = 1,
    Pong = 2,
    DebugCommandRequest = 100,
    DebugCommandResponse = 101,
};

enum class ServerPacketType : uint16_t
{
    Ping = 1,
    Pong = 2,
    ZoneRegister = 10,
    ZoneRegisterAck = 11,
    DebugCommandRequest = 100,
    DebugCommandResponse = 101,
};

} // namespace tbeq::net
