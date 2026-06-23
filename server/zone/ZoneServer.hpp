#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <asio.hpp>

#include "debug/DebugCommandHandler.hpp"
#include "net/TcpConnection.hpp"
#include "tbeq/core/Config.hpp"
#include "tbeq/persistence/Database.hpp"
#include "tbeq/world/TileDefCatalog.hpp"
#include "tbeq/world/ZoneGrid.hpp"

namespace tbeq::server
{

class ZoneServer
{
public:
    ZoneServer(asio::io_context& io, const AppConfig& config);

    uint16_t clientPort() const { return clientPort_; }
    bool connectAndRegister();

private:
    struct PlayerEntity
    {
        std::string characterId;
        std::string name;
        uint64_t sessionTokenHash = 0;
        int32_t tileX = 0;
        int32_t tileY = 0;
        uint32_t entityId = 0;
        std::shared_ptr<TcpConnection> connection;
        bool connected = false;
        bool locationDirty = false;
    };

    struct NpcEntity
    {
        std::string npcId;
        std::string slotType;
        int32_t tileX = 0;
        int32_t tileY = 0;
        uint32_t entityId = 0;
    };

    struct PendingTransfer
    {
        std::string characterId;
        uint64_t sessionTokenHash = 0;
        std::shared_ptr<TcpConnection> clientConnection;
        uint32_t clientSequenceId = 0;
    };

    bool loadZoneData();
    void spawnNpcEntities();
    uint32_t allocateEntityId();

    void handleWorldPacket(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);
    void handleClientPacket(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);

    void handlePlayerEnterPrepare(
        const std::shared_ptr<TcpConnection>& connection,
        const net::SerializedPacket& packet);
    void handleZoneTransferAuthorize(const net::SerializedPacket& packet);

    void sendClientPacket(
        const std::shared_ptr<TcpConnection>& connection,
        net::ClientPacketType type,
        uint32_t sequenceId,
        const net::ByteWriter& writer,
        uint64_t sessionTokenHash = 0) const;

    void sendServerPacketToWorld(
        const std::shared_ptr<TcpConnection>& connection,
        net::ServerPacketType type,
        uint32_t sequenceId,
        const net::ByteWriter& writer) const;

    void broadcastEntitySnapshot(uint32_t excludeEntityId = 0);
    net::EntitySnapshotPayload buildEntitySnapshot(uint32_t excludeEntityId = 0) const;
    net::ZoneSnapshotPayload buildZoneSnapshot(bool includeTiles = false) const;
    net::ZoneTileGridPayload buildZoneTileGrid() const;

    void flushPlayerLocation(PlayerEntity& player);

    PlayerEntity* findPlayerByConnection(const std::shared_ptr<TcpConnection>& connection);
    PlayerEntity* findPlayerByCharacterId(const std::string& characterId);
    void removePlayer(const std::string& characterId);

    void handleSessionResume(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);
    void handleRequestZoneTiles(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);
    void handleMoveIntent(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);
    void handleChatMessage(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);
    void handleUsePortal(const std::shared_ptr<TcpConnection>& connection, const net::SerializedPacket& packet);

    void deliverSayChat(const PlayerEntity& sender, const std::string& text);

    asio::io_context& io_;
    AppConfig config_;
    DebugCommandHandler debugHandler_;
    db::Database database_;
    world::TileDefCatalog tileDefs_;
    world::ZoneGrid zoneGrid_;

    TcpAcceptor clientAcceptor_;
    std::shared_ptr<TcpConnection> worldLink_;
    uint16_t clientPort_ = 0;

    std::string pendingCharacterId_;
    uint32_t pendingSequenceId_ = 0;
    uint64_t pendingSessionResumeToken_ = 0;

    mutable std::mutex stateMutex_;
    std::unordered_map<std::string, PlayerEntity> players_;
    std::vector<NpcEntity> npcs_;
    std::unordered_map<std::string, PendingTransfer> pendingTransfers_;
    uint32_t nextEntityId_ = 1;
};

} // namespace tbeq::server
