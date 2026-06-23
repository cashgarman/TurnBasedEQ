#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <span>
#include <vector>

#include <asio.hpp>

#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::server
{

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    using PacketHandler = std::function<void(std::shared_ptr<TcpConnection>, const net::SerializedPacket&)>;

    TcpConnection(asio::ip::tcp::socket socket, PacketHandler handler);

    void start();
    void send(const net::SerializedPacket& packet);
    void close();

private:
    void readHeader();
    void readPayload();
    void doWrite();
    void handleError(const std::error_code& ec, const char* where);

    asio::ip::tcp::socket socket_;
    PacketHandler handler_;
    net::PacketHeader readHeader_;
    std::vector<uint8_t> readBuffer_;
    std::deque<std::vector<uint8_t>> writeQueue_;
    bool writing_ = false;
    bool closed_ = false;
};

class TcpAcceptor
{
public:
    TcpAcceptor(asio::io_context& io, uint16_t port, TcpConnection::PacketHandler handler);

    uint16_t port() const { return acceptor_.local_endpoint().port(); }

private:
    void doAccept();

    asio::io_context& io_;
    asio::ip::tcp::acceptor acceptor_;
    TcpConnection::PacketHandler handler_;
};

} // namespace tbeq::server
