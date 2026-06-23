#include "net/TcpConnection.hpp"

#include <spdlog/spdlog.h>

namespace tbeq::server
{

TcpConnection::TcpConnection(asio::ip::tcp::socket socket, PacketHandler handler)
    : socket_(std::move(socket))
    , handler_(std::move(handler))
{
}

void TcpConnection::start()
{
    readHeader();
}

void TcpConnection::send(const net::SerializedPacket& packet)
{
    writeQueue_.push_back(net::encodePacket(packet));
    if (!writing_)
    {
        doWrite();
    }
}

void TcpConnection::doWrite()
{
    if (closed_ || writeQueue_.empty())
    {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto self = shared_from_this();
    asio::async_write(
        socket_,
        asio::buffer(writeQueue_.front()),
        [self](const std::error_code& ec, std::size_t)
        {
            if (ec)
            {
                self->handleError(ec, "write");
                return;
            }

            self->writeQueue_.pop_front();
            self->doWrite();
        });
}

void TcpConnection::setCloseHandler(CloseHandler handler)
{
    closeHandler_ = std::move(handler);
}

void TcpConnection::notifyClosed()
{
    if (closeNotified_)
    {
        return;
    }
    closeNotified_ = true;
    if (closeHandler_)
    {
        closeHandler_(shared_from_this());
    }
}

void TcpConnection::close()
{
    if (closed_)
    {
        return;
    }
    closed_ = true;
    writing_ = false;
    writeQueue_.clear();
    std::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    notifyClosed();
}

void TcpConnection::readHeader()
{
    auto self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(&readHeader_, sizeof(readHeader_)),
        [self](const std::error_code& ec, std::size_t)
        {
            if (ec)
            {
                self->handleError(ec, "readHeader");
                return;
            }
            if (!self->readHeader_.isValid())
            {
                spdlog::warn("Invalid packet header received");
                self->close();
                return;
            }
            self->readPayload();
        });
}

void TcpConnection::readPayload()
{
    auto self = shared_from_this();
    readBuffer_.resize(readHeader_.payloadLength);
    if (readBuffer_.empty())
    {
        net::SerializedPacket packet;
        packet.header = readHeader_;
        if (handler_)
        {
            handler_(self, packet);
        }
        readHeader();
        return;
    }

    asio::async_read(
        socket_,
        asio::buffer(readBuffer_),
        [self](const std::error_code& ec, std::size_t)
        {
            if (ec)
            {
                self->handleError(ec, "readPayload");
                return;
            }

            net::SerializedPacket packet;
            packet.header = self->readHeader_;
            packet.payload = self->readBuffer_;
            if (self->handler_)
            {
                self->handler_(self, packet);
            }
            self->readHeader();
        });
}

void TcpConnection::handleError(const std::error_code& ec, const char* where)
{
    if (ec != asio::error::eof && ec != asio::error::operation_aborted)
    {
        spdlog::debug("TcpConnection {} error: {}", where, ec.message());
    }
    writing_ = false;
    writeQueue_.clear();
    close();
}

TcpAcceptor::TcpAcceptor(
    asio::io_context& io,
    uint16_t port,
    TcpConnection::PacketHandler handler,
    ConnectionHandler connectionHandler)
    : io_(io)
    , acceptor_(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    , handler_(std::move(handler))
    , connectionHandler_(std::move(connectionHandler))
{
    doAccept();
}

void TcpAcceptor::doAccept()
{
    acceptor_.async_accept(
        [this](const std::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (!ec)
            {
                auto connection = std::make_shared<TcpConnection>(std::move(socket), handler_);
                if (connectionHandler_)
                {
                    connectionHandler_(connection);
                }
                connection->start();
            }
            doAccept();
        });
}

} // namespace tbeq::server
