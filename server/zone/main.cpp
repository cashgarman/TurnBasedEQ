#include "debug/DebugCommandHandler.hpp"
#include "net/TcpConnection.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include <asio.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/core/Config.hpp"
#include "tbeq/core/Log.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"

namespace
{

bool sendPacket(asio::ip::tcp::socket& socket, const tbeq::net::SerializedPacket& packet)
{
    const auto encoded = tbeq::net::encodePacket(packet);
    std::error_code ec;
    asio::write(socket, asio::buffer(encoded), ec);
    return !ec;
}

bool readPacket(asio::ip::tcp::socket& socket, tbeq::net::SerializedPacket& packet)
{
    tbeq::net::PacketHeader header{};
    std::error_code ec;
    asio::read(socket, asio::buffer(&header, sizeof(header)), ec);
    if (ec || !header.isValid())
    {
        return false;
    }

    std::vector<uint8_t> payload(header.payloadLength);
    if (!payload.empty())
    {
        asio::read(socket, asio::buffer(payload), ec);
        if (ec)
        {
            return false;
        }
    }

    packet.header = header;
    packet.payload = std::move(payload);
    return true;
}

class ZoneServerApp
{
public:
    ZoneServerApp(asio::io_context& io, const tbeq::AppConfig& config)
        : io_(io)
        , config_(config)
        , debugHandler_(config.devMode)
        , clientAcceptor_(io, config.zoneClientPort, [](std::shared_ptr<tbeq::server::TcpConnection>,
                                                          const tbeq::net::SerializedPacket&)
        {
            spdlog::debug("Client packet received (stub handler)");
        })
    {
        clientPort_ = clientAcceptor_.port();
        spdlog::info("ZoneServer client listener on port {}", clientPort_);
        if (config.devMode)
        {
            spdlog::warn("ZoneServer running with --dev-mode ENABLED");
        }
    }

    uint16_t clientPort() const { return clientPort_; }

    bool connectAndRegister()
    {
        asio::ip::tcp::resolver resolver(io_);
        auto endpoints = resolver.resolve(config_.worldLoginHost, std::to_string(config_.worldLoginPort));
        asio::ip::tcp::socket socket(io_);
        asio::connect(socket, endpoints);

        tbeq::net::ZoneRegisterPayload payload;
        payload.zoneId = config_.zoneId;
        payload.listenHost = "127.0.0.1";
        payload.listenPort = clientPort_;
        payload.capacity = 100;
        payload.version = "0.1.0";

        auto writer = tbeq::net::serialize(payload);
        auto packet = tbeq::net::serializeServerPacket(
            tbeq::net::ServerPacketType::ZoneRegister,
            1,
            writer);

        if (!sendPacket(socket, packet))
        {
            return false;
        }

        tbeq::net::SerializedPacket response;
        if (!readPacket(socket, response))
        {
            return false;
        }

        tbeq::net::ZoneRegisterAckPayload ack;
        if (!tbeq::net::deserializeServerPacket(response, ack) || !ack.accepted)
        {
            return false;
        }

        spdlog::info("ZoneRegisterAck message={}", ack.message);

        worldLink_ = std::make_shared<tbeq::server::TcpConnection>(
            std::move(socket),
            [this](std::shared_ptr<tbeq::server::TcpConnection>, const tbeq::net::SerializedPacket& packet)
            {
                handleWorldPacket(packet);
            });
        worldLink_->start();
        return true;
    }

private:
    void handleWorldPacket(const tbeq::net::SerializedPacket& packet)
    {
        const auto type = static_cast<tbeq::net::ServerPacketType>(packet.header.packetType);
        switch (type)
        {
        case tbeq::net::ServerPacketType::Pong:
        {
            tbeq::net::ServerPongPayload pong;
            if (tbeq::net::deserializeServerPacket(packet, pong))
            {
                spdlog::debug("Pong timestamp={}", pong.timestampMs);
            }
            break;
        }
        case tbeq::net::ServerPacketType::DebugCommandResponse:
        {
            tbeq::net::ServerDebugCommandResponsePayload response;
            if (tbeq::net::deserializeServerPacket(packet, response))
            {
                spdlog::info("DebugCommandResponse ok={} message={}", response.ok, response.message);
            }
            break;
        }
        default:
            spdlog::warn("Unhandled packet from WorldLogin type={}", packet.header.packetType);
            break;
        }
    }

    asio::io_context& io_;
    tbeq::AppConfig config_;
    tbeq::server::DebugCommandHandler debugHandler_;
    tbeq::server::TcpAcceptor clientAcceptor_;
    std::shared_ptr<tbeq::server::TcpConnection> worldLink_;
    uint16_t clientPort_ = 0;
};

} // namespace

int main(int argc, char** argv)
{
    tbeq::initLogging("zone_server");
    auto config = tbeq::parseServerArgs(argc, argv, "zone_server");

    try
    {
        asio::io_context io;
        ZoneServerApp app(io, config);
        std::cout << "TBEQ_ZONE_CLIENT_PORT=" << app.clientPort() << std::endl;

        if (!app.connectAndRegister())
        {
            spdlog::error("Failed to register with WorldLogin");
            return 1;
        }

        spdlog::info("ZoneServer registered successfully for zone {}", config.zoneId);
        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("ZoneServer fatal error: {}", ex.what());
        return 1;
    }

    return 0;
}
