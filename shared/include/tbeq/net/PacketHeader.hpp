#pragma once

#include <cstdint>
#include <cstring>

namespace tbeq::net
{

constexpr uint32_t kPacketMagic = 0x54424551; // 'TBEQ'
constexpr uint16_t kProtocolVersion = 1;
constexpr std::size_t kMaxPayloadSize = 1024 * 1024;

#pragma pack(push, 1)
struct PacketHeader
{
    uint32_t magic = kPacketMagic;
    uint16_t protocolVersion = kProtocolVersion;
    uint16_t packetType = 0;
    uint32_t sequenceId = 0;
    uint32_t payloadLength = 0;
    uint64_t sessionTokenHash = 0;

    bool isValid() const
    {
        return magic == kPacketMagic
            && protocolVersion == kProtocolVersion
            && payloadLength <= kMaxPayloadSize;
    }
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 24, "PacketHeader must be 24 bytes");

} // namespace tbeq::net
