#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketHeader.hpp"
#include "tbeq/net/ServerPackets.hpp"

namespace tbeq::net
{

class ByteWriter
{
public:
    void writeU8(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeU64(uint64_t value);
    void writeBool(bool value);
    void writeString(const std::string& value);
    void writeStringVector(const std::vector<std::string>& values);

    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t>&& take() { return std::move(buffer_); }

private:
    std::vector<uint8_t> buffer_;
};

class ByteReader
{
public:
    explicit ByteReader(std::span<const uint8_t> data);

    bool hasRemaining() const;
    bool readU8(uint8_t& out);
    bool readU16(uint16_t& out);
    bool readU32(uint32_t& out);
    bool readU64(uint64_t& out);
    bool readBool(bool& out);
    bool readString(std::string& out);
    bool readStringVector(std::vector<std::string>& out);

private:
    std::span<const uint8_t> data_;
    std::size_t offset_ = 0;
};

struct SerializedPacket
{
    PacketHeader header;
    std::vector<uint8_t> payload;
};

SerializedPacket serializeClientPacket(ClientPacketType type, uint32_t sequenceId, const ByteWriter& writer);
SerializedPacket serializeServerPacket(ServerPacketType type, uint32_t sequenceId, const ByteWriter& writer);

bool deserializeClientPacket(const SerializedPacket& packet, ClientPingPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientPongPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandResponsePayload& out);

bool deserializeServerPacket(const SerializedPacket& packet, ServerPingPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerPongPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterAckPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandRequestPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandResponsePayload& out);

ByteWriter serialize(const ClientPingPayload& payload);
ByteWriter serialize(const ClientPongPayload& payload);
ByteWriter serialize(const DebugCommandRequestPayload& payload);
ByteWriter serialize(const DebugCommandResponsePayload& payload);

ByteWriter serialize(const ServerPingPayload& payload);
ByteWriter serialize(const ServerPongPayload& payload);
ByteWriter serialize(const ZoneRegisterPayload& payload);
ByteWriter serialize(const ZoneRegisterAckPayload& payload);

bool readPacketHeader(std::span<const uint8_t> data, PacketHeader& header);
std::vector<uint8_t> encodePacket(const SerializedPacket& packet);
bool decodePacket(std::span<const uint8_t> data, SerializedPacket& out);

} // namespace tbeq::net
