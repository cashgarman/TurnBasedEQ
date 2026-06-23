#include "HeadlessClient.hpp"

#include <chrono>
#include <cstring>
#include <thread>

namespace tbeq::test
{

HeadlessClient::HeadlessClient(asio::io_context& io)
    : io_(io)
    , socket_(io)
{
}

void HeadlessClient::connect(const std::string& host, uint16_t port)
{
    asio::ip::tcp::resolver resolver(io_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    asio::connect(socket_, endpoints);
}

void HeadlessClient::close()
{
    std::error_code ec;
    socket_.close(ec);
}

void HeadlessClient::sendServerPacket(net::ServerPacketType type, uint32_t sequenceId, const net::ByteWriter& payloadWriter)
{
    const auto packet = net::serializeServerPacket(type, sequenceId, payloadWriter);
    const auto encoded = net::encodePacket(packet);
    asio::write(socket_, asio::buffer(encoded));
    (void)nextSequence_;
}

std::optional<net::SerializedPacket> HeadlessClient::readServerPacket(std::chrono::milliseconds timeout)
{
    std::vector<uint8_t> headerBuffer(sizeof(net::PacketHeader));
    if (!readExact(sizeof(net::PacketHeader), headerBuffer, timeout))
    {
        return std::nullopt;
    }

    net::PacketHeader header{};
    std::memcpy(&header, headerBuffer.data(), sizeof(header));
    if (!header.isValid())
    {
        return std::nullopt;
    }

    std::vector<uint8_t> payload(header.payloadLength);
    if (!payload.empty() && !readExact(payload.size(), payload, timeout))
    {
        return std::nullopt;
    }

    net::SerializedPacket packet;
    packet.header = header;
    packet.payload = std::move(payload);
    return packet;
}

bool HeadlessClient::readExact(std::size_t size, std::vector<uint8_t>& buffer, std::chrono::milliseconds timeout)
{
    buffer.resize(size);
    std::size_t read = 0;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (read < size)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }

        std::error_code ec;
        const std::size_t bytes = socket_.read_some(asio::buffer(buffer.data() + read, size - read), ec);
        if (ec)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        read += bytes;
    }

    return true;
}

} // namespace tbeq::test
