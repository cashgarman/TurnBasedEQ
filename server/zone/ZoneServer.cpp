#include "ZoneServer.hpp"

#include <cmath>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"
#include "tbeq/social/ChatChannel.hpp"

namespace tbeq::server
{

namespace
{

constexpr int32_t kSayRadiusTiles = 15;

} // namespace

ZoneServer::ZoneServer(asio::io_context& io, const AppConfig& config)
    : io_(io)
    , config_(config)
    , debugHandler_(config.devMode)
    , database_(config.dbPath)
    , clientAcceptor_(io, config.zoneClientPort, [this](std::shared_ptr<TcpConnection> connection,
                                                        const net::SerializedPacket& packet)
    {
        handleClientPacket(std::move(connection), packet);
    })
{
    clientPort_ = clientAcceptor_.port();
    if (!database_.open())
    {
        throw std::runtime_error("Failed to open zone database");
    }

    const auto tileDefsPath = std::filesystem::path(config.dataRoot) / "tile_defs.json";
    if (!tileDefs_.loadFromFile(tileDefsPath))
    {
        throw std::runtime_error("Failed to load tile_defs.json");
    }

    if (!loadZoneData())
    {
        throw std::runtime_error("Failed to load zone data from database");
    }

    spawnNpcEntities();

    spdlog::info(
        "ZoneServer {} loaded {}x{} with {} NPC slots",
        config.zoneId,
        zoneGrid_.width(),
        zoneGrid_.height(),
        npcs_.size());
    spdlog::info("ZoneServer client listener on port {}", clientPort_);
    if (config.devMode)
    {
        spdlog::warn("ZoneServer running with --dev-mode ENABLED");
    }
}

bool ZoneServer::loadZoneData()
{
    return zoneGrid_.loadFromDatabase(database_, config_.zoneId, tileDefs_);
}

void ZoneServer::spawnNpcEntities()
{
    npcs_.clear();
    for (const auto& slot : database_.loadZoneNpcSlots(config_.zoneId))
    {
        NpcEntity npc;
        npc.npcId = slot.npcId.empty() ? slot.slotType : slot.npcId;
        npc.slotType = slot.slotType;
        npc.tileX = slot.tileX;
        npc.tileY = slot.tileY;
        npc.entityId = allocateEntityId();
        npcs_.push_back(std::move(npc));
    }
}

uint32_t ZoneServer::allocateEntityId()
{
    return nextEntityId_++;
}

bool ZoneServer::connectAndRegister()
{
    asio::ip::tcp::resolver resolver(io_);
    auto endpoints = resolver.resolve(config_.worldLoginHost, std::to_string(config_.worldLoginPort));
    asio::ip::tcp::socket socket(io_);
    asio::connect(socket, endpoints);

    net::ZoneRegisterPayload payload;
    payload.zoneId = config_.zoneId;
    payload.listenHost = "127.0.0.1";
    payload.listenPort = clientPort_;
    payload.capacity = 100;
    payload.version = "0.2.0";

    auto writer = net::serialize(payload);
    auto packet = net::serializeServerPacket(net::ServerPacketType::ZoneRegister, 1, writer);

    const auto encoded = net::encodePacket(packet);
    std::error_code ec;
    asio::write(socket, asio::buffer(encoded), ec);
    if (ec)
    {
        return false;
    }

    net::PacketHeader header{};
    asio::read(socket, asio::buffer(&header, sizeof(header)), ec);
    if (ec || !header.isValid())
    {
        return false;
    }

    std::vector<uint8_t> responsePayload(header.payloadLength);
    if (!responsePayload.empty())
    {
        asio::read(socket, asio::buffer(responsePayload), ec);
        if (ec)
        {
            return false;
        }
    }

    net::SerializedPacket response;
    response.header = header;
    response.payload = std::move(responsePayload);

    net::ZoneRegisterAckPayload ack;
    if (!net::deserializeServerPacket(response, ack) || !ack.accepted)
    {
        return false;
    }

    spdlog::info("ZoneRegisterAck message={}", ack.message);

    worldLink_ = std::make_shared<TcpConnection>(
        std::move(socket),
        [this](std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet)
        {
            handleWorldPacket(connection, packet);
        });
    worldLink_->start();
    return true;
}

void ZoneServer::sendClientPacket(
    const std::shared_ptr<TcpConnection>& connection,
    net::ClientPacketType type,
    uint32_t sequenceId,
    const net::ByteWriter& writer,
    uint64_t sessionTokenHash) const
{
    if (!connection)
    {
        return;
    }
    auto packet = net::serializeClientPacket(type, sequenceId, writer);
    packet.header.sessionTokenHash = sessionTokenHash;
    connection->send(packet);
}

void ZoneServer::sendServerPacketToWorld(
    const std::shared_ptr<TcpConnection>& connection,
    net::ServerPacketType type,
    uint32_t sequenceId,
    const net::ByteWriter& writer) const
{
    if (connection)
    {
        connection->send(net::serializeServerPacket(type, sequenceId, writer));
    }
}

net::ZoneSnapshotPayload ZoneServer::buildZoneSnapshot(bool includeTiles) const
{
    net::ZoneSnapshotPayload snapshot;
    snapshot.zoneId = zoneGrid_.zoneId();
    snapshot.zoneName = zoneGrid_.zoneName();
    snapshot.tileStyle = zoneGrid_.tileStyle();
    snapshot.width = zoneGrid_.width();
    snapshot.height = zoneGrid_.height();
    if (includeTiles)
    {
        snapshot.tiles = zoneGrid_.tiles();
    }
    return snapshot;
}

net::ZoneTileGridPayload ZoneServer::buildZoneTileGrid() const
{
    net::ZoneTileGridPayload grid;
    grid.zoneId = zoneGrid_.zoneId();
    grid.tiles = zoneGrid_.tiles();
    return grid;
}

void ZoneServer::flushPlayerLocation(PlayerEntity& player)
{
    if (!player.locationDirty)
    {
        return;
    }

    database_.updateCharacterLocation(
        player.characterId,
        config_.zoneId,
        static_cast<float>(player.tileX),
        static_cast<float>(player.tileY));
    player.locationDirty = false;
}

net::EntitySnapshotPayload ZoneServer::buildEntitySnapshot(uint32_t excludeEntityId) const
{
    net::EntitySnapshotPayload snapshot;
    std::lock_guard<std::mutex> lock(stateMutex_);

    for (const auto& npc : npcs_)
    {
        if (npc.entityId == excludeEntityId)
        {
            continue;
        }
        net::EntityStatePayload entity;
        entity.entityId = npc.entityId;
        entity.name = npc.npcId;
        entity.entityType = 1;
        entity.tileX = npc.tileX;
        entity.tileY = npc.tileY;
        snapshot.entities.push_back(std::move(entity));
    }

    for (const auto& [_, player] : players_)
    {
        if (player.entityId == excludeEntityId)
        {
            continue;
        }
        net::EntityStatePayload entity;
        entity.entityId = player.entityId;
        entity.name = player.name;
        entity.entityType = 0;
        entity.tileX = player.tileX;
        entity.tileY = player.tileY;
        snapshot.entities.push_back(std::move(entity));
    }

    return snapshot;
}

void ZoneServer::broadcastEntitySnapshot(uint32_t excludeEntityId)
{
    const auto snapshot = buildEntitySnapshot(excludeEntityId);
    const auto writer = net::serialize(snapshot);

    std::vector<std::pair<std::shared_ptr<TcpConnection>, uint64_t>> recipients;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (const auto& [_, player] : players_)
        {
            if (!player.connected || !player.connection)
            {
                continue;
            }
            recipients.emplace_back(player.connection, player.sessionTokenHash);
        }
    }

    for (const auto& [connection, sessionTokenHash] : recipients)
    {
        sendClientPacket(
            connection,
            net::ClientPacketType::EntitySnapshot,
            0,
            writer,
            sessionTokenHash);
    }
}

ZoneServer::PlayerEntity* ZoneServer::findPlayerByConnection(const std::shared_ptr<TcpConnection>& connection)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    for (auto& [_, player] : players_)
    {
        if (player.connection == connection)
        {
            return &player;
        }
    }
    return nullptr;
}

ZoneServer::PlayerEntity* ZoneServer::findPlayerByCharacterId(const std::string& characterId)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto it = players_.find(characterId);
    if (it == players_.end())
    {
        return nullptr;
    }
    return &it->second;
}

void ZoneServer::removePlayer(const std::string& characterId)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto it = players_.find(characterId);
    if (it != players_.end())
    {
        flushPlayerLocation(it->second);
        players_.erase(it);
    }
}

void ZoneServer::handleWorldPacket(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    const auto type = static_cast<net::ServerPacketType>(packet.header.packetType);
    switch (type)
    {
    case net::ServerPacketType::PlayerEnterPrepare:
        handlePlayerEnterPrepare(connection, packet);
        break;
    case net::ServerPacketType::ZoneTransferAuthorize:
        handleZoneTransferAuthorize(packet);
        break;
    case net::ServerPacketType::ZoneConnectForward:
    {
        net::ZoneConnectForwardPayload forward;
        if (!net::deserializeServerPacket(packet, forward))
        {
            return;
        }

        PendingTransfer transfer;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            const auto it = pendingTransfers_.find(forward.characterId);
            if (it == pendingTransfers_.end())
            {
                spdlog::warn("ZoneConnectForward for unknown transfer {}", forward.characterId);
                return;
            }
            transfer = std::move(it->second);
            pendingTransfers_.erase(it);
        }

        net::ZoneConnectInfoPayload connectInfo;
        connectInfo.ok = forward.ok;
        connectInfo.message = forward.message;
        connectInfo.zoneId = forward.zoneId;
        connectInfo.zoneHost = forward.zoneHost;
        connectInfo.zonePort = forward.zonePort;
        connectInfo.sessionResumeToken = forward.sessionResumeToken;
        connectInfo.characterId = forward.characterId;

        sendClientPacket(
            transfer.clientConnection,
            net::ClientPacketType::ZoneConnectInfo,
            transfer.clientSequenceId,
            net::serialize(connectInfo),
            forward.sessionResumeToken);
        break;
    }
    case net::ServerPacketType::LoadCharacterResponse:
    {
        net::LoadCharacterResponsePayload loadResponse;
        if (!net::deserializeServerPacket(packet, loadResponse))
        {
            return;
        }

        net::PlayerEnterReadyPayload ready;
        ready.characterId = loadResponse.characterId.empty() ? pendingCharacterId_ : loadResponse.characterId;
        ready.ok = loadResponse.ok;
        ready.message = loadResponse.ok ? "ready" : loadResponse.message;

        if (loadResponse.ok)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            PlayerEntity player;
            player.characterId = loadResponse.characterId;
            player.name = loadResponse.name;
            player.tileX = static_cast<int32_t>(std::lround(loadResponse.posX));
            player.tileY = static_cast<int32_t>(std::lround(loadResponse.posY));
            player.entityId = allocateEntityId();
            player.sessionTokenHash = pendingSessionResumeToken_;
            players_[player.characterId] = std::move(player);
        }

        sendServerPacketToWorld(
            connection,
            net::ServerPacketType::PlayerEnterReady,
            pendingSequenceId_,
            net::serialize(ready));
        break;
    }
    case net::ServerPacketType::Pong:
    {
        net::ServerPongPayload pong;
        if (net::deserializeServerPacket(packet, pong))
        {
            spdlog::debug("Pong timestamp={}", pong.timestampMs);
        }
        break;
    }
    case net::ServerPacketType::DebugCommandResponse:
    {
        net::ServerDebugCommandResponsePayload response;
        if (net::deserializeServerPacket(packet, response))
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

void ZoneServer::handlePlayerEnterPrepare(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::PlayerEnterPreparePayload prepare;
    if (!net::deserializeServerPacket(packet, prepare))
    {
        return;
    }

    spdlog::info(
        "PlayerEnterPrepare character={} zone={}",
        prepare.characterId,
        prepare.zoneId);

    net::LoadCharacterRequestPayload loadRequest;
    loadRequest.characterId = prepare.characterId;
    auto loadWriter = net::serialize(loadRequest);
    connection->send(net::serializeServerPacket(
        net::ServerPacketType::LoadCharacterRequest,
        packet.header.sequenceId,
        loadWriter));

    pendingCharacterId_ = prepare.characterId;
    pendingSequenceId_ = packet.header.sequenceId;
    pendingSessionResumeToken_ = prepare.sessionResumeToken;
}

void ZoneServer::handleZoneTransferAuthorize(const net::SerializedPacket& packet)
{
    net::ZoneTransferAuthorizePayload authorize;
    if (!net::deserializeServerPacket(packet, authorize))
    {
        return;
    }

    spdlog::info("ZoneTransferAuthorize character={}", authorize.characterId);
    removePlayer(authorize.characterId);
    broadcastEntitySnapshot();
}

void ZoneServer::handleClientPacket(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    const auto type = static_cast<net::ClientPacketType>(packet.header.packetType);
    switch (type)
    {
    case net::ClientPacketType::SessionResume:
        handleSessionResume(connection, packet);
        break;
    case net::ClientPacketType::RequestZoneTiles:
        handleRequestZoneTiles(connection, packet);
        break;
    case net::ClientPacketType::MoveIntent:
        handleMoveIntent(connection, packet);
        break;
    case net::ClientPacketType::ChatMessage:
        handleChatMessage(connection, packet);
        break;
    case net::ClientPacketType::UsePortal:
        handleUsePortal(connection, packet);
        break;
    case net::ClientPacketType::Ping:
    {
        net::ClientPingPayload ping;
        if (net::deserializeClientPacket(packet, ping))
        {
            net::ClientPongPayload pong;
            pong.timestampMs = ping.timestampMs;
            sendClientPacket(
                connection,
                net::ClientPacketType::Pong,
                packet.header.sequenceId,
                net::serialize(pong),
                packet.header.sessionTokenHash);
        }
        break;
    }
    default:
        spdlog::warn("Unhandled client packet type={}", packet.header.packetType);
        break;
    }
}

void ZoneServer::handleSessionResume(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::SessionResumePayload request;
    if (!net::deserializeClientPacket(packet, request))
    {
        return;
    }

    net::SessionResumeResponsePayload response;
    PlayerEntity* player = findPlayerByCharacterId(request.characterId);
    if (player == nullptr)
    {
        response.ok = false;
        response.message = "Character not in zone";
    }
    else if (player->sessionTokenHash != 0 && player->sessionTokenHash != request.sessionResumeToken)
    {
        response.ok = false;
        response.message = "Invalid session token";
    }
    else
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        player->connection = connection;
        player->connected = true;
        player->sessionTokenHash = request.sessionResumeToken;
        response.ok = true;
        response.message = "Session resumed";
        response.entityId = player->entityId;
        response.tileX = player->tileX;
        response.tileY = player->tileY;
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::SessionResumeResponse,
        packet.header.sequenceId,
        net::serialize(response),
        request.sessionResumeToken);

    if (!response.ok)
    {
        return;
    }

    const uint32_t selfEntityId = player->entityId;

    sendClientPacket(
        connection,
        net::ClientPacketType::ZoneSnapshot,
        packet.header.sequenceId + 1,
        net::serialize(buildZoneSnapshot(false)),
        request.sessionResumeToken);

    sendClientPacket(
        connection,
        net::ClientPacketType::EntitySnapshot,
        packet.header.sequenceId + 2,
        net::serialize(buildEntitySnapshot()),
        request.sessionResumeToken);

    broadcastEntitySnapshot(selfEntityId);
}

void ZoneServer::handleRequestZoneTiles(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::RequestZoneTilesPayload request;
    if (!net::deserializeClientPacket(packet, request))
    {
        return;
    }

    if (findPlayerByConnection(connection) == nullptr)
    {
        return;
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::ZoneTileGrid,
        packet.header.sequenceId,
        net::serialize(buildZoneTileGrid()),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleMoveIntent(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::MoveIntentPayload intent;
    net::MoveResultPayload result;
    if (!net::deserializeClientPacket(packet, intent))
    {
        result.message = "Invalid move intent";
        sendClientPacket(
            connection,
            net::ClientPacketType::MoveResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr)
    {
        result.message = "Not in zone";
    }
    else if (!zoneGrid_.canStepTo(player->tileX, player->tileY, intent.targetTileX, intent.targetTileY))
    {
        result.message = "Invalid move";
    }
    else
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        player->tileX = intent.targetTileX;
        player->tileY = intent.targetTileY;
        player->locationDirty = true;
        result.ok = true;
        result.message = "moved";
        result.entityId = player->entityId;
        result.tileX = player->tileX;
        result.tileY = player->tileY;
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::MoveResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);

    if (result.ok)
    {
        broadcastEntitySnapshot();
    }
}

void ZoneServer::deliverSayChat(const PlayerEntity& sender, const std::string& text)
{
    net::ChatDeliverPayload deliver;
    deliver.channel = static_cast<uint8_t>(ChatChannel::Say);
    deliver.senderName = sender.name;
    deliver.text = text;
    const auto writer = net::serialize(deliver);

    std::vector<std::pair<std::shared_ptr<TcpConnection>, uint64_t>> recipients;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (const auto& [_, player] : players_)
        {
            if (!player.connected || !player.connection)
            {
                continue;
            }

            const int32_t dx = std::abs(player.tileX - sender.tileX);
            const int32_t dy = std::abs(player.tileY - sender.tileY);
            if (dx > kSayRadiusTiles || dy > kSayRadiusTiles)
            {
                continue;
            }

            recipients.emplace_back(player.connection, player.sessionTokenHash);
        }
    }

    for (const auto& [connection, sessionTokenHash] : recipients)
    {
        sendClientPacket(
            connection,
            net::ClientPacketType::ChatDeliver,
            0,
            writer,
            sessionTokenHash);
    }
}

void ZoneServer::handleChatMessage(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::ChatMessagePayload message;
    if (!net::deserializeClientPacket(packet, message))
    {
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr || message.text.empty())
    {
        return;
    }

    if (static_cast<ChatChannel>(message.channel) == ChatChannel::Say)
    {
        deliverSayChat(*player, message.text);
    }
}

void ZoneServer::handleUsePortal(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    PlayerEntity* player = findPlayerByConnection(connection);
    net::UsePortalResultPayload result;
    if (player == nullptr)
    {
        result.message = "Not in zone";
        sendClientPacket(
            connection,
            net::ClientPacketType::UsePortalResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    const auto portal = zoneGrid_.portalAt(player->tileX, player->tileY);
    if (!portal.has_value())
    {
        result.message = "No portal here";
        sendClientPacket(
            connection,
            net::ClientPacketType::UsePortalResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    if (!worldLink_)
    {
        result.message = "World link unavailable";
        sendClientPacket(
            connection,
            net::ClientPacketType::UsePortalResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    flushPlayerLocation(*player);

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        pendingTransfers_[player->characterId] = PendingTransfer{
            player->characterId,
            player->sessionTokenHash,
            connection,
            packet.header.sequenceId};
    }

    net::ZoneTransferRequestPayload transfer;
    transfer.characterId = player->characterId;
    transfer.sourceZoneId = config_.zoneId;
    transfer.destZoneId = portal->destZoneId;
    transfer.destTileX = portal->destX;
    transfer.destTileY = portal->destY;
    transfer.sessionResumeToken = player->sessionTokenHash;

    sendServerPacketToWorld(
        worldLink_,
        net::ServerPacketType::ZoneTransferRequest,
        packet.header.sequenceId,
        net::serialize(transfer));

    result.ok = true;
    result.message = "Transfer requested";
    sendClientPacket(
        connection,
        net::ClientPacketType::UsePortalResult,
        packet.header.sequenceId,
        net::serialize(result),
        player->sessionTokenHash);
}

} // namespace tbeq::server
