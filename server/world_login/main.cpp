#include "debug/DebugCommandHandler.hpp"
#include "net/TcpConnection.hpp"

#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <asio.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/core/Config.hpp"
#include "tbeq/core/Log.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"

namespace
{

struct ZoneEntry
{
    tbeq::net::ZoneRegisterPayload payload;
    std::shared_ptr<tbeq::server::TcpConnection> connection;
};

class WorldLoginServer
{
public:
    WorldLoginServer(asio::io_context& io, const tbeq::AppConfig& config)
        : io_(io)
        , config_(config)
        , debugHandler_(config.devMode)
        , zoneAcceptor_(io, config.worldLoginPort, [this](std::shared_ptr<tbeq::server::TcpConnection> connection,
                                                          const tbeq::net::SerializedPacket& packet)
        {
            handleZonePacket(std::move(connection), packet);
        })
    {
        actualPort_ = zoneAcceptor_.port();
        spdlog::info("WorldLogin listening for zone servers on port {}", actualPort_);
        if (config.devMode)
        {
            spdlog::warn("WorldLogin running with --dev-mode ENABLED");
        }
    }

    uint16_t port() const { return actualPort_; }

private:
    void handleZonePacket(std::shared_ptr<tbeq::server::TcpConnection> connection, const tbeq::net::SerializedPacket& packet)
    {
        const auto type = static_cast<tbeq::net::ServerPacketType>(packet.header.packetType);
        switch (type)
        {
        case tbeq::net::ServerPacketType::ZoneRegister:
        {
            tbeq::net::ZoneRegisterPayload payload;
            if (!tbeq::net::deserializeServerPacket(packet, payload))
            {
                spdlog::warn("Failed to deserialize ZoneRegister");
                return;
            }

            spdlog::info(
                "ZoneRegister: id={} host={} port={} capacity={} version={}",
                payload.zoneId,
                payload.listenHost,
                payload.listenPort,
                payload.capacity,
                payload.version);

            {
                std::lock_guard<std::mutex> lock(zonesMutex_);
                zones_[payload.zoneId] = ZoneEntry{payload, connection};
            }

            tbeq::net::ZoneRegisterAckPayload ack;
            ack.accepted = true;
            ack.message = "registered";
            auto writer = tbeq::net::serialize(ack);
            auto response = tbeq::net::serializeServerPacket(
                tbeq::net::ServerPacketType::ZoneRegisterAck,
                packet.header.sequenceId,
                writer);

            if (connection)
            {
                connection->send(response);
            }
            break;
        }
        case tbeq::net::ServerPacketType::Ping:
        {
            tbeq::net::ServerPingPayload ping;
            if (!tbeq::net::deserializeServerPacket(packet, ping))
            {
                return;
            }
            tbeq::net::ServerPongPayload pong;
            pong.timestampMs = ping.timestampMs;
            auto writer = tbeq::net::serialize(pong);
            auto response = tbeq::net::serializeServerPacket(
                tbeq::net::ServerPacketType::Pong,
                packet.header.sequenceId,
                writer);
            if (connection)
            {
                connection->send(response);
            }
            break;
        }
        case tbeq::net::ServerPacketType::DebugCommandRequest:
        {
            tbeq::net::ServerDebugCommandRequestPayload request;
            if (!tbeq::net::deserializeServerPacket(packet, request))
            {
                return;
            }
            auto result = debugHandler_.handle(request);
            tbeq::net::ServerDebugCommandResponsePayload responsePayload;
            responsePayload.ok = result.ok;
            responsePayload.message = result.message;
            auto writer = tbeq::net::serialize(responsePayload);
            auto response = tbeq::net::serializeServerPacket(
                tbeq::net::ServerPacketType::DebugCommandResponse,
                packet.header.sequenceId,
                writer);
            if (connection)
            {
                connection->send(response);
            }
            break;
        }
        default:
            spdlog::warn("Unhandled server packet type {}", packet.header.packetType);
            break;
        }
    }

    asio::io_context& io_;
    tbeq::AppConfig config_;
    tbeq::server::DebugCommandHandler debugHandler_;
    tbeq::server::TcpAcceptor zoneAcceptor_;
    uint16_t actualPort_ = 0;
    std::mutex zonesMutex_;
    std::unordered_map<std::string, ZoneEntry> zones_;
};

} // namespace

int main(int argc, char** argv)
{
    tbeq::initLogging("world_login");
    auto config = tbeq::parseServerArgs(argc, argv, "world_login");

    try
    {
        asio::io_context io;
        WorldLoginServer server(io, config);
        std::cout << "TBEQ_WORLD_LOGIN_PORT=" << server.port() << std::endl;
        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("WorldLogin fatal error: {}", ex.what());
        return 1;
    }

    return 0;
}
