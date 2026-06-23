#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <asio.hpp>

#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::test
{

class HeadlessClient
{
public:
    explicit HeadlessClient(asio::io_context& io);

    void connect(const std::string& host, uint16_t port);
    void close();

    void sendServerPacket(net::ServerPacketType type, uint32_t sequenceId, const net::ByteWriter& payloadWriter);
    std::optional<net::SerializedPacket> readServerPacket(std::chrono::milliseconds timeout);

    void sendClientPacket(
        net::ClientPacketType type,
        uint32_t sequenceId,
        const net::ByteWriter& payloadWriter,
        uint64_t sessionTokenHash = 0);
    std::optional<net::SerializedPacket> readClientPacket(std::chrono::milliseconds timeout);

private:
    bool readExact(std::size_t size, std::vector<uint8_t>& buffer, std::chrono::milliseconds timeout);

    asio::io_context& io_;
    asio::ip::tcp::socket socket_;
    uint32_t nextSequence_ = 1;
};

} // namespace tbeq::test
