#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <asio.hpp>
#include <nlohmann/json.hpp>

#include "debug/DebugCommandHandler.hpp"
#include "net/TcpConnection.hpp"
#include "tbeq/core/Config.hpp"
#include "tbeq/persistence/Database.hpp"

namespace tbeq::server
{

class WorldLoginServer
{
public:
    WorldLoginServer(asio::io_context& io, const AppConfig& config);

    uint16_t zonePort() const { return zonePort_; }
    uint16_t clientPort() const { return clientPort_; }

private:
    struct ZoneEntry
    {
        net::ZoneRegisterPayload payload;
        std::shared_ptr<TcpConnection> connection;
    };

    struct PendingHandoff
    {
        std::shared_ptr<TcpConnection> clientConnection;
        uint32_t clientSequenceId = 0;
        uint64_t sessionTokenHash = 0;
        std::string characterId;
    };

    struct PendingZoneTransfer
    {
        std::string sourceZoneId;
        std::string destZoneId;
        uint64_t sessionTokenHash = 0;
        int32_t destTileX = 0;
        int32_t destTileY = 0;
    };

    bool bootstrapDatabase();
    bool ensureWorldGenerated();
    nlohmann::json loadJsonArray(const std::filesystem::path& path) const;

    void handleZonePacket(std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet);
    void handleClientPacket(std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet);

    std::optional<int64_t> validateSession(uint64_t sessionTokenHash) const;
    uint64_t issueSession(int64_t accountId);
    uint64_t sessionTokenHash(uint64_t token) const;
    uint64_t generateTokenValue();

    void sendClientPacket(
        const std::shared_ptr<TcpConnection>& connection,
        net::ClientPacketType type,
        uint32_t sequenceId,
        const net::ByteWriter& writer,
        uint64_t sessionTokenHash = 0) const;

    void sendServerPacketToZone(
        const std::shared_ptr<TcpConnection>& connection,
        net::ServerPacketType type,
        uint32_t sequenceId,
        const net::ByteWriter& writer) const;

    std::shared_ptr<TcpConnection> findZoneConnection(const std::string& zoneId) const;

    asio::io_context& io_;
    AppConfig config_;
    DebugCommandHandler debugHandler_;
    db::Database database_;
    nlohmann::json racesJson_;
    nlohmann::json classesJson_;

    TcpAcceptor zoneAcceptor_;
    TcpAcceptor clientAcceptor_;
    uint16_t zonePort_ = 0;
    uint16_t clientPort_ = 0;

    mutable std::mutex zonesMutex_;
    std::unordered_map<std::string, ZoneEntry> zones_;

    mutable std::mutex handoffMutex_;
    std::unordered_map<std::string, PendingHandoff> pendingHandoffs_;

    mutable std::mutex transferMutex_;
    std::unordered_map<std::string, PendingZoneTransfer> pendingZoneTransfers_;
};

} // namespace tbeq::server
