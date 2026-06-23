#include "tbeq/net/PacketSerializer.hpp"

#include <cstring>
#include <stdexcept>

namespace tbeq::net
{

void appendRaw(std::vector<uint8_t>& buffer, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

void ByteWriter::writeU8(uint8_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU16(uint16_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU32(uint32_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU64(uint64_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeBool(bool value)
{
    writeU8(value ? 1 : 0);
}

void ByteWriter::writeString(const std::string& value)
{
    writeU32(static_cast<uint32_t>(value.size()));
    appendRaw(buffer_, value.data(), value.size());
}

void ByteWriter::writeStringVector(const std::vector<std::string>& values)
{
    writeU32(static_cast<uint32_t>(values.size()));
    for (const auto& value : values)
    {
        writeString(value);
    }
}

ByteReader::ByteReader(std::span<const uint8_t> data)
    : data_(data)
{
}

bool ByteReader::hasRemaining() const
{
    return offset_ < data_.size();
}

bool ByteReader::readU8(uint8_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU16(uint16_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU32(uint32_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU64(uint64_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readBool(bool& out)
{
    uint8_t value = 0;
    if (!readU8(value))
    {
        return false;
    }
    out = value != 0;
    return true;
}

bool ByteReader::readString(std::string& out)
{
    uint32_t length = 0;
    if (!readU32(length) || offset_ + length > data_.size())
    {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(data_.data() + offset_), length);
    offset_ += length;
    return true;
}

bool ByteReader::readStringVector(std::vector<std::string>& out)
{
    uint32_t count = 0;
    if (!readU32(count))
    {
        return false;
    }
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        std::string value;
        if (!readString(value))
        {
            return false;
        }
        out.push_back(std::move(value));
    }
    return true;
}

SerializedPacket serializeClientPacket(ClientPacketType type, uint32_t sequenceId, const ByteWriter& writer)
{
    SerializedPacket packet;
    packet.header.packetType = static_cast<uint16_t>(type);
    packet.header.sequenceId = sequenceId;
    packet.payload = writer.data();
    packet.header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    return packet;
}

SerializedPacket serializeServerPacket(ServerPacketType type, uint32_t sequenceId, const ByteWriter& writer)
{
    SerializedPacket packet;
    packet.header.packetType = static_cast<uint16_t>(type);
    packet.header.sequenceId = sequenceId;
    packet.payload = writer.data();
    packet.header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    return packet;
}

ByteWriter serialize(const ClientPingPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ClientPongPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const DebugCommandRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeU16(static_cast<uint16_t>(payload.command));
    writer.writeStringVector(payload.args);
    return writer;
}

ByteWriter serialize(const DebugCommandResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const ServerPingPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ServerPongPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ZoneRegisterPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.zoneId);
    writer.writeString(payload.listenHost);
    writer.writeU16(payload.listenPort);
    writer.writeU32(payload.capacity);
    writer.writeString(payload.version);
    return writer;
}

ByteWriter serialize(const ZoneRegisterAckPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.accepted);
    writer.writeString(payload.message);
    return writer;
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientPingPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::Ping))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientPongPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::Pong))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::DebugCommandRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint16_t command = 0;
    if (!reader.readU16(command))
    {
        return false;
    }
    out.command = static_cast<DebugCommand>(command);
    return reader.readStringVector(out.args);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::DebugCommandResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerPingPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::Ping))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerPongPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::Pong))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneRegister))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.zoneId)
        && reader.readString(out.listenHost)
        && reader.readU16(out.listenPort)
        && reader.readU32(out.capacity)
        && reader.readString(out.version);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterAckPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneRegisterAck))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.accepted) && reader.readString(out.message);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::DebugCommandRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint16_t command = 0;
    if (!reader.readU16(command))
    {
        return false;
    }
    out.command = static_cast<DebugCommand>(command);
    return reader.readStringVector(out.args);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::DebugCommandResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool readPacketHeader(std::span<const uint8_t> data, PacketHeader& header)
{
    if (data.size() < sizeof(PacketHeader))
    {
        return false;
    }
    std::memcpy(&header, data.data(), sizeof(PacketHeader));
    return header.isValid();
}

std::vector<uint8_t> encodePacket(const SerializedPacket& packet)
{
    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(PacketHeader) + packet.payload.size());
    PacketHeader header = packet.header;
    header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    std::memcpy(buffer.data(), &header, sizeof(PacketHeader));
    if (!packet.payload.empty())
    {
        std::memcpy(buffer.data() + sizeof(PacketHeader), packet.payload.data(), packet.payload.size());
    }
    return buffer;
}

bool decodePacket(std::span<const uint8_t> data, SerializedPacket& out)
{
    if (!readPacketHeader(data, out.header))
    {
        return false;
    }
    if (data.size() < sizeof(PacketHeader) + out.header.payloadLength)
    {
        return false;
    }
    out.payload.assign(
        data.begin() + sizeof(PacketHeader),
        data.begin() + sizeof(PacketHeader) + out.header.payloadLength);
    return true;
}

} // namespace tbeq::net
