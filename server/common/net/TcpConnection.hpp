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
    using CloseHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

    TcpConnection(asio::ip::tcp::socket socket, PacketHandler handler);

    void start();
    void send(const net::SerializedPacket& packet);
    void close();
    void setCloseHandler(CloseHandler handler);

private:
    void readHeader();
    void readPayload();
    void doWrite();
    void handleError(const std::error_code& ec, const char* where);
    void notifyClosed();

    asio::ip::tcp::socket socket_;
    PacketHandler handler_;
    CloseHandler closeHandler_;
    bool closeNotified_ = false;
    net::PacketHeader readHeader_;
    std::vector<uint8_t> readBuffer_;
    std::deque<std::vector<uint8_t>> writeQueue_;
    bool writing_ = false;
    bool closed_ = false;
};

class TcpAcceptor
{
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

    TcpAcceptor(
        asio::io_context& io,
        uint16_t port,
        TcpConnection::PacketHandler handler,
        ConnectionHandler connectionHandler = {});

    uint16_t port() const { return acceptor_.local_endpoint().port(); }

private:
    void doAccept();

    asio::io_context& io_;
    asio::ip::tcp::acceptor acceptor_;
    TcpConnection::PacketHandler handler_;
    ConnectionHandler connectionHandler_;
};

} // namespace tbeq::server
