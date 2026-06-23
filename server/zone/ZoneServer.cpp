#include "ZoneServer.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "tbeq/combat/CombatTypes.hpp"
#include "tbeq/items/ItemRules.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"
#include "tbeq/social/ChatChannel.hpp"
#include "tbeq/skills/Progression.hpp"
#include "tbeq/worldgen/WorldBootstrap.hpp"

namespace tbeq::server
{

namespace
{

constexpr int32_t kSayRadiusTiles = 15;
constexpr int32_t kAggroRadiusTiles = 3;

std::optional<std::pair<int32_t, int32_t>> findNearestWalkableTileInGrid(
    const world::ZoneGrid& grid,
    int32_t centerX,
    int32_t centerY)
{
    const int32_t maxRadius = std::max(grid.width(), grid.height());
    for (int32_t radius = 0; radius <= maxRadius; ++radius)
    {
        for (int32_t dy = -radius; dy <= radius; ++dy)
        {
            for (int32_t dx = -radius; dx <= radius; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != radius)
                {
                    continue;
                }

                const int32_t x = centerX + dx;
                const int32_t y = centerY + dy;
                if (grid.isWalkable(x, y))
                {
                    return std::make_pair(x, y);
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace

ZoneServer::ZoneServer(asio::io_context& io, const AppConfig& config)
    : io_(io)
    , config_(config)
    , debugHandler_(config.devMode)
    , database_(config.dbPath)
    , clientAcceptor_(io, config.zoneClientPort,
        [this](std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet)
        {
            handleClientPacket(std::move(connection), packet);
        },
        [this](const std::shared_ptr<TcpConnection>& connection)
        {
            connection->setCloseHandler(
                [this](const std::shared_ptr<TcpConnection>& closedConnection)
                {
                    handleClientDisconnect(closedConnection);
                });
        })
{
    clientPort_ = clientAcceptor_.port();
    if (!database_.open())
    {
        throw std::runtime_error("Failed to open zone database");
    }

    if (!worldgen::ensureWorldInDatabase(
            database_,
            config_.worldSeed,
            std::filesystem::path(config_.dataRoot)))
    {
        throw std::runtime_error("Failed to ensure world data in database");
    }

    if (!worldgen::ensureZoneGenerated(
            database_,
            config_.zoneId,
            config_.worldSeed,
            std::filesystem::path(config_.dataRoot)))
    {
        throw std::runtime_error("Failed to generate zone content");
    }

    const auto tileDefsPath = std::filesystem::path(config.dataRoot) / "tile_defs.json";
    if (!tileDefs_.loadFromFile(tileDefsPath))
    {
        throw std::runtime_error("Failed to load tile_defs.json");
    }

    const auto mobsPath = std::filesystem::path(config.dataRoot) / "mobs.json";
    if (!mobCatalog_.loadFromFile(mobsPath))
    {
        throw std::runtime_error("Failed to load mobs.json");
    }

    const auto skillsPath = std::filesystem::path(config.dataRoot) / "skills.json";
    if (!skillCatalog_.loadFromFile(skillsPath))
    {
        throw std::runtime_error("Failed to load skills.json");
    }

    const auto skillCapsPath = std::filesystem::path(config.dataRoot) / "skill_caps.json";
    if (!skillCapCatalog_.loadFromFile(skillCapsPath))
    {
        throw std::runtime_error("Failed to load skill_caps.json");
    }
    skillResolver_.setSkillCapCatalog(&skillCapCatalog_);

    const auto bossScriptsPath = std::filesystem::path(config.dataRoot) / "boss_scripts.json";
    if (!bossScriptCatalog_.loadFromFile(bossScriptsPath))
    {
        throw std::runtime_error("Failed to load boss_scripts.json");
    }

    const auto spellsPath = std::filesystem::path(config.dataRoot) / "spells.json";
    if (!spellCatalog_.loadFromFile(spellsPath))
    {
        throw std::runtime_error("Failed to load spells.json");
    }

    const auto abilitiesPath = std::filesystem::path(config.dataRoot) / "abilities.json";
    if (!abilityCatalog_.loadFromFile(abilitiesPath))
    {
        throw std::runtime_error("Failed to load abilities.json");
    }

    const auto aiProfilesPath = std::filesystem::path(config.dataRoot) / "ai" / "class_combat_profiles.json";
    if (!aiProfiles_.loadFromFile(aiProfilesPath))
    {
        throw std::runtime_error("Failed to load class_combat_profiles.json");
    }

    const auto itemsPath = std::filesystem::path(config.dataRoot) / "items.json";
    if (!itemCatalog_.loadFromFile(itemsPath))
    {
        throw std::runtime_error("Failed to load items.json");
    }

    const auto npcsPath = std::filesystem::path(config.dataRoot) / "npcs.json";
    if (!npcCatalog_.loadFromFile(npcsPath))
    {
        throw std::runtime_error("Failed to load npcs.json");
    }

    if (!loadZoneData())
    {
        throw std::runtime_error("Failed to load zone data from database");
    }

    spawnNpcEntities();
    loadZoneSpawns();

    combatManager_ = std::make_unique<CombatManager>(
        io_,
        skillResolver_,
        mobCatalog_,
        spellCatalog_,
        abilityCatalog_,
        bossScriptCatalog_,
        skillCatalog_,
        aiProfiles_,
        [this](const std::string& characterId) -> CombatManager::PlayerView*
        {
            if (PlayerEntity* player = findPlayerByCharacterId(characterId))
            {
                combatViewScratch_ = makePlayerView(*player);
                return &combatViewScratch_;
            }
            if (AiPartyMember* ai = findAiByCharacterId(characterId))
            {
                combatViewScratch_ = makeAiView(*ai);
                return &combatViewScratch_;
            }
            return nullptr;
        },
        [this](const std::string& leaderCharacterId)
        {
            return findAiCompanionsForLeader(leaderCharacterId);
        },
        [this](const std::vector<std::string>& characterIds, net::ClientPacketType type, const net::ByteWriter& writer)
        {
            broadcastToPlayers(characterIds, type, writer);
        },
        [this](const std::string& characterId, const CharacterState& state)
        {
            persistPlayerState(characterId, state);
        },
        [this](const std::string& characterId)
        {
            if (PlayerEntity* player = findPlayerByCharacterId(characterId))
            {
                sendInventorySnapshot(*player);
            }
        },
        [this](const std::string& characterId)
        {
            if (PlayerEntity* player = findPlayerByCharacterId(characterId))
            {
                sendSkillsSnapshot(*player);
            }
        });

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

void ZoneServer::loadZoneSpawns()
{
    spawns_.clear();
    for (const auto& record : database_.loadZoneSpawns(config_.zoneId))
    {
        ZoneSpawnState spawn;
        spawn.spawnId = record.spawnId;
        spawn.tileX = record.tileX;
        spawn.tileY = record.tileY;
        spawn.mobTable = record.mobTable;
        spawn.respawnSeconds = record.respawnSeconds;
        spawn.active = true;
        spawn.entityId = allocateEntityId();
        spawns_.push_back(std::move(spawn));
    }
}

void ZoneServer::spawnNpcEntities()
{
    npcs_.clear();
    for (const auto& slot : database_.loadZoneNpcSlots(config_.zoneId))
    {
        NpcEntity npc;
        npc.npcId = npcCatalog_.resolveNpcId(slot.npcId.empty() ? slot.slotType : slot.npcId);
        npc.slotType = slot.slotType;
        npc.tileX = slot.tileX;
        npc.tileY = slot.tileY;
        npc.entityId = allocateEntityId();

        const content::NpcDef* npcDef = npcCatalog_.findNpc(npc.npcId);
        if (npcDef != nullptr)
        {
            for (const auto& buyEntry : npcDef->merchantBuyStock)
            {
                npc.merchantStockRemaining[buyEntry.itemId] = buyEntry.quantity;
            }
        }

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

void ZoneServer::persistPlayerToDatabase(const PlayerEntity& player)
{
    database_.updateCharacterState(player.characterId, player.characterState.toJson());
    database_.updateCharacterLocation(
        player.characterId,
        config_.zoneId,
        static_cast<float>(player.tileX),
        static_cast<float>(player.tileY));
}

std::string ZoneServer::evictPlayerForConnection(const std::shared_ptr<TcpConnection>& connection)
{
    std::string characterId;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (auto it = players_.begin(); it != players_.end(); ++it)
        {
            if (it->second.connection == connection)
            {
                characterId = it->first;
                persistPlayerToDatabase(it->second);
                players_.erase(it);
                break;
            }
        }
    }
    return characterId;
}

void ZoneServer::finalizePlayerEviction(const std::string& characterId)
{
    if (characterId.empty())
    {
        return;
    }

    spdlog::info("Client disconnected character={} zone={}", characterId, config_.zoneId);
    sendPlayerDisconnectToWorld(characterId);
    broadcastEntitySnapshot();
}

void ZoneServer::sendPlayerDisconnectToWorld(const std::string& characterId)
{
    if (!worldLink_)
    {
        return;
    }

    net::PlayerDisconnectPayload payload;
    payload.characterId = characterId;
    sendServerPacketToWorld(
        worldLink_,
        net::ServerPacketType::PlayerDisconnect,
        0,
        net::serialize(payload));
}

void ZoneServer::handleClientDisconnect(const std::shared_ptr<TcpConnection>& connection)
{
    finalizePlayerEviction(evictPlayerForConnection(connection));
}

void ZoneServer::handleSessionEnd(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::SessionEndPayload request;
    if (!net::deserializeClientPacket(packet, request))
    {
        return;
    }

    finalizePlayerEviction(evictPlayerForConnection(connection));
}

void ZoneServer::sendZoneTransferCompleteToWorld(const PlayerEntity& player)
{
    if (!worldLink_)
    {
        return;
    }

    net::ZoneTransferCompletePayload payload;
    payload.characterId = player.characterId;
    payload.zoneId = config_.zoneId;
    payload.posX = static_cast<float>(player.tileX);
    payload.posY = static_cast<float>(player.tileY);
    sendServerPacketToWorld(
        worldLink_,
        net::ServerPacketType::ZoneTransferComplete,
        0,
        net::serialize(payload));
}

void ZoneServer::removePlayer(const std::string& characterId)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto it = players_.find(characterId);
    if (it != players_.end())
    {
        persistPlayerToDatabase(it->second);
        players_.erase(it);
    }
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
        const content::NpcDef* npcDef = npcCatalog_.findNpc(npc.npcId);
        entity.name = npcDef != nullptr ? npcDef->name : npc.npcId;
        entity.entityType = 1;
        entity.tileX = npc.tileX;
        entity.tileY = npc.tileY;
        entity.appearanceId = npc.slotType.empty() ? npc.npcId : npc.slotType;
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
        entity.raceId = player.raceId;
        entity.classId = player.classId;
        entity.equippedWeaponItemId = player.characterState.equippedItemInSlot("weapon");
        entity.equippedHeadItemId = player.characterState.equippedItemInSlot("head");
        entity.equippedChestItemId = player.characterState.equippedItemInSlot("chest");
        entity.equippedHandsItemId = player.characterState.equippedItemInSlot("hands");
        snapshot.entities.push_back(std::move(entity));
    }

    for (const auto& [_, ai] : aiCompanions_)
    {
        if (ai.entityId == excludeEntityId)
        {
            continue;
        }
        net::EntityStatePayload entity;
        entity.entityId = ai.entityId;
        entity.name = ai.name;
        entity.entityType = 0;
        entity.tileX = ai.tileX;
        entity.tileY = ai.tileY;
        entity.classId = ai.classId;
        entity.appearanceId = "ai_companion";
        snapshot.entities.push_back(std::move(entity));
    }

    for (const auto& spawn : spawns_)
    {
        if (!spawn.active)
        {
            continue;
        }

        const auto mobIds = mobCatalog_.resolveMobTable(spawn.mobTable);
        if (mobIds.empty())
        {
            continue;
        }

        const content::MobDef* mobDef = mobCatalog_.findMob(mobIds.front());
        net::EntityStatePayload entity;
        entity.entityId = spawn.entityId;
        entity.name = mobDef != nullptr ? mobDef->name : mobIds.front();
        entity.entityType = 2;
        entity.tileX = spawn.tileX;
        entity.tileY = spawn.tileY;
        entity.appearanceId = mobIds.front();
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
            player.raceId = loadResponse.raceId;
            player.classId = loadResponse.classId;
            player.level = loadResponse.level;
            player.characterState = CharacterState::fromJson(loadResponse.stateJson);
            if (player.characterState.skills.empty())
            {
                player.characterState = CharacterState::createDefault(player.classId, player.level);
            }
            if (player.characterState.classId.empty())
            {
                player.characterState.classId = player.classId;
            }
            player.level = player.characterState.level;
            items::ItemRules::recomputeDerivedStats(player.characterState, itemCatalog_);
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

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        players_.erase(prepare.characterId);
    }

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
    case net::ClientPacketType::SubmitAction:
        handleSubmitAction(connection, packet);
        break;
    case net::ClientPacketType::MeditateRequest:
        handleMeditate(connection, packet);
        break;
    case net::ClientPacketType::DebugCommandRequest:
        handleDebugCommand(connection, packet);
        break;
    case net::ClientPacketType::EquipItemRequest:
        handleEquipItem(connection, packet);
        break;
    case net::ClientPacketType::UnequipItemRequest:
        handleUnequipItem(connection, packet);
        break;
    case net::ClientPacketType::NpcInteractRequest:
        handleNpcInteract(connection, packet);
        break;
    case net::ClientPacketType::MerchantBuyRequest:
        handleMerchantBuy(connection, packet);
        break;
    case net::ClientPacketType::MerchantSellRequest:
        handleMerchantSell(connection, packet);
        break;
    case net::ClientPacketType::SessionEnd:
        handleSessionEnd(connection, packet);
        break;
    case net::ClientPacketType::PlayerCommandRequest:
        handlePlayerCommand(connection, packet);
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

    sendInventorySnapshot(*player);
    sendSkillsSnapshot(*player);

    broadcastEntitySnapshot(selfEntityId);
    sendZoneTransferCompleteToWorld(*player);
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

    if (result.ok && player != nullptr)
    {
        updateSpawnRespawns();
        tryAggro(*player);
        for (auto& [_, ai] : aiCompanions_)
        {
            if (ai.leaderCharacterId == player->characterId)
            {
                ai.tileX = player->tileX;
                ai.tileY = player->tileY;
            }
        }
        broadcastEntitySnapshot();
    }
}

void ZoneServer::updateSpawnRespawns()
{
    const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (auto& spawn : spawns_)
        {
            if (spawn.active || spawn.respawnAtUnix <= 0)
            {
                continue;
            }
            if (now >= spawn.respawnAtUnix)
            {
                spawn.active = true;
                spawn.respawnAtUnix = 0;
                changed = true;
            }
        }
    }

    if (changed)
    {
        broadcastEntitySnapshot();
    }
}

CombatManager::PlayerView ZoneServer::makePlayerView(PlayerEntity& player)
{
    CombatManager::PlayerView view;
    view.characterId = player.characterId;
    view.name = player.name;
    view.classId = player.classId;
    view.raceId = player.raceId;
    view.level = player.level;
    view.state = &player.characterState;
    view.inCombat = &player.inCombat;
    view.combatId = &player.combatId;
    view.combatSlot = &player.combatSlot;
    view.isAiCompanion = false;
    return view;
}

CombatManager::PlayerView ZoneServer::makeAiView(AiPartyMember& ai)
{
    CombatManager::PlayerView view;
    view.characterId = ai.characterId;
    view.name = ai.name;
    view.classId = ai.classId;
    view.raceId = "human";
    view.level = ai.level;
    view.state = &ai.characterState;
    view.inCombat = &ai.inCombat;
    view.combatId = &ai.combatId;
    view.combatSlot = &ai.combatSlot;
    view.isAiCompanion = true;
    return view;
}

std::vector<CombatManager::PlayerView> ZoneServer::findAiCompanionsForLeader(const std::string& leaderCharacterId)
{
    std::vector<CombatManager::PlayerView> views;
    for (auto& [_, ai] : aiCompanions_)
    {
        if (ai.leaderCharacterId != leaderCharacterId)
        {
            continue;
        }
        views.push_back(makeAiView(ai));
    }
    return views;
}

ZoneServer::AiPartyMember* ZoneServer::findAiByCharacterId(const std::string& characterId)
{
    const auto it = aiCompanions_.find(characterId);
    if (it == aiCompanions_.end())
    {
        return nullptr;
    }
    return &it->second;
}

bool ZoneServer::spawnAiCompanion(
    PlayerEntity& leader,
    const std::string& classId,
    uint16_t level,
    std::string& outMessage)
{
    const std::string resolvedClass = classId.empty() ? "cleric" : classId;
    const uint16_t resolvedLevel = level == 0 ? leader.level : level;

    AiPartyMember ai;
    ai.characterId = "ai_" + std::to_string(nextAiId_++);
    ai.classId = resolvedClass;
    ai.level = resolvedLevel;
    ai.characterState = CharacterState::createDefault(resolvedClass, resolvedLevel);
    ai.characterState.classId = resolvedClass;
    ai.characterState.level = resolvedLevel;
    ai.leaderCharacterId = leader.characterId;
    ai.entityId = allocateEntityId();
    ai.tileX = leader.tileX;
    ai.tileY = leader.tileY;
    ai.name = resolvedClass.substr(0, 1) == "c" ? "Cleric Companion" : resolvedClass + " Companion";
    if (resolvedClass == "cleric")
    {
        ai.name = "Cleric Companion";
    }

    const std::string aiId = ai.characterId;
    const std::string aiName = ai.name;
    aiCompanions_.emplace(aiId, std::move(ai));
    outMessage = "Spawned AI companion: " + aiName;
    broadcastEntitySnapshot();
    return true;
}

void ZoneServer::persistPlayerState(const std::string& characterId, const CharacterState& state)
{
    database_.updateCharacterState(characterId, state.toJson());
}

void ZoneServer::broadcastToPlayers(
    const std::vector<std::string>& characterIds,
    net::ClientPacketType type,
    const net::ByteWriter& writer)
{
    std::vector<std::pair<std::shared_ptr<TcpConnection>, uint64_t>> recipients;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (const auto& characterId : characterIds)
        {
            const auto it = players_.find(characterId);
            if (it == players_.end() || !it->second.connected || !it->second.connection)
            {
                continue;
            }
            recipients.emplace_back(it->second.connection, it->second.sessionTokenHash);
        }
    }

    for (const auto& [connection, sessionTokenHash] : recipients)
    {
        sendClientPacket(connection, type, 0, writer, sessionTokenHash);
    }
}

void ZoneServer::tryAggro(PlayerEntity& player)
{
    if (player.inCombat || combatManager_ == nullptr)
    {
        return;
    }

    CombatManager::PlayerView view;
    std::string mobTable;
    ZoneSpawnState* aggroSpawn = nullptr;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (player.inCombat)
        {
            return;
        }

        for (auto& spawn : spawns_)
        {
            if (!spawn.active)
            {
                continue;
            }

            const int32_t dx = std::abs(player.tileX - spawn.tileX);
            const int32_t dy = std::abs(player.tileY - spawn.tileY);
            if (dx > kAggroRadiusTiles || dy > kAggroRadiusTiles)
            {
                continue;
            }

            view = makePlayerView(player);
            mobTable = spawn.mobTable;
            aggroSpawn = &spawn;
            break;
        }
    }

    if (aggroSpawn == nullptr)
    {
        return;
    }

    if (!combatManager_->tryStartSpawnCombat(view, mobTable))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        aggroSpawn->active = false;
        aggroSpawn->respawnAtUnix = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
            + aggroSpawn->respawnSeconds;
        spdlog::info(
            "Combat started for {} at spawn {} ({}, {})",
            player.characterId,
            aggroSpawn->spawnId,
            aggroSpawn->tileX,
            aggroSpawn->tileY);
    }

    broadcastEntitySnapshot();
}

void ZoneServer::handleSubmitAction(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::SubmitActionPayload request;
    net::SubmitActionResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid submit action";
        sendClientPacket(
            connection,
            net::ClientPacketType::SubmitActionResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr || combatManager_ == nullptr)
    {
        result.message = "Not in zone";
    }
    else
    {
        combatManager_->submitAction(player->characterId, request, result);
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::SubmitActionResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleMeditate(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::MeditateResultPayload result;
    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr)
    {
        result.message = "Not in zone";
    }
    else if (player->inCombat)
    {
        result.message = "Cannot meditate in combat";
    }
    else if (player->characterState.maxMana == 0)
    {
        result.message = "You have no mana to restore";
    }
    else
    {
        const uint16_t meditateSkill = player->characterState.skillLevel("meditate");
        const uint16_t gain = skillResolver_.meditateManaGain(meditateSkill, player->characterState.maxMana);
        const uint16_t before = player->characterState.mana;
        player->characterState.mana = static_cast<uint16_t>(std::min(
            static_cast<uint32_t>(player->characterState.maxMana),
            static_cast<uint32_t>(player->characterState.mana) + gain));
        result.ok = true;
        result.manaGained = static_cast<uint16_t>(player->characterState.mana - before);
        result.mana = player->characterState.mana;
        result.maxMana = player->characterState.maxMana;
        result.message = "You meditate and recover " + std::to_string(result.manaGained) + " mana.";
        persistPlayerState(player->characterId, player->characterState);
        applyActivitySkillGain(*player, "meditate", "meditate");
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::MeditateResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleDebugCommand(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::ClientDebugCommandRequestPayload request;
    net::ClientDebugCommandResponsePayload response;
    if (!net::deserializeClientPacket(packet, request))
    {
        response.message = "Invalid debug command";
        sendClientPacket(
            connection,
            net::ClientPacketType::DebugCommandResponse,
            packet.header.sequenceId,
            net::serialize(response),
            packet.header.sessionTokenHash);
        return;
    }

    if (!debugHandler_.devModeEnabled())
    {
        response.message = "Debug commands rejected: dev-mode is disabled";
        sendClientPacket(
            connection,
            net::ClientPacketType::DebugCommandResponse,
            packet.header.sequenceId,
            net::serialize(response),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    switch (request.command)
    {
    case net::DebugCommand::SpawnMob:
    {
        if (player == nullptr || combatManager_ == nullptr)
        {
            response.message = "Not in zone";
            break;
        }

        std::vector<std::string> mobIds;
        if (!request.args.empty())
        {
            const auto tableMobs = mobCatalog_.resolveMobTable(request.args[0]);
            if (!tableMobs.empty())
            {
                mobIds = tableMobs;
            }
            else
            {
                mobIds.push_back(request.args[0]);
            }
        }
        else
        {
            mobIds.push_back("forest_rat");
        }

        auto view = makePlayerView(*player);
        response.ok = combatManager_->startDebugCombat(view, mobIds);
        response.message = response.ok ? "Combat started" : "Failed to start combat";
        break;
    }
    case net::DebugCommand::KillTarget:
    {
        if (player == nullptr || combatManager_ == nullptr || request.args.empty())
        {
            response.message = "Usage: kill_target <combatSlot>";
            break;
        }
        const uint32_t targetSlot = static_cast<uint32_t>(std::stoul(request.args[0]));
        response.ok = combatManager_->killTargetInCombat(player->characterId, targetSlot);
        response.message = response.ok ? "Target killed" : "Kill target failed";
        break;
    }
    case net::DebugCommand::GodMode:
    {
        if (player == nullptr || combatManager_ == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        const bool enabled = request.args.empty() || request.args[0] != "off";
        combatManager_->setGodMode(player->characterId, enabled);
        response.ok = true;
        response.message = enabled ? "God mode enabled" : "God mode disabled";
        break;
    }
    case net::DebugCommand::ForceCombatEnd:
    {
        if (player == nullptr || combatManager_ == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        response.ok = combatManager_->forceEndCombat(player->characterId, combat::CombatOutcome::Victory);
        response.message = response.ok ? "Combat ended" : "Not in combat";
        break;
    }
    case net::DebugCommand::UnlockAllSpells:
    {
        if (player == nullptr || combatManager_ == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        combatManager_->unlockAllSpells(player->characterId);
        response.ok = true;
        response.message = "Unlocked all class spells and abilities";
        break;
    }
    case net::DebugCommand::FillMana:
    {
        if (player == nullptr || combatManager_ == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        combatManager_->fillMana(player->characterId);
        response.ok = true;
        response.message = "Mana filled";
        break;
    }
    case net::DebugCommand::SpawnAi:
    {
        if (player == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        std::string classId = "cleric";
        uint16_t level = player->level;
        if (!request.args.empty())
        {
            classId = request.args[0];
        }
        if (request.args.size() >= 2)
        {
            level = static_cast<uint16_t>(std::stoul(request.args[1]));
        }
        response.ok = spawnAiCompanion(*player, classId, level, response.message);
        break;
    }
    case net::DebugCommand::GrantItem:
    {
        if (player == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        if (request.args.empty())
        {
            response.message = "Usage: grant_item <itemId> [quantity]";
            break;
        }
        const content::ItemDef* item = itemCatalog_.findItem(request.args[0]);
        if (item == nullptr)
        {
            response.message = "Unknown item id";
            break;
        }
        uint16_t quantity = 1;
        if (request.args.size() >= 2)
        {
            quantity = static_cast<uint16_t>(std::stoul(request.args[1]));
        }
        player->characterState.addItem(item->id, quantity);
        persistPlayerState(player->characterId, player->characterState);
        sendInventorySnapshot(*player);
        response.ok = true;
        response.message = "Granted " + std::to_string(quantity) + " x " + item->name;
        break;
    }
    case net::DebugCommand::EquipItem:
    {
        if (player == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        if (request.args.empty())
        {
            response.message = "Usage: equip_item <itemId>";
            break;
        }
        response.ok = tryEquipItem(*player, request.args[0], response.message);
        break;
    }
    case net::DebugCommand::SetSkillLevel:
    {
        if (player == nullptr || request.args.size() < 2)
        {
            response.message = "Usage: set_skill <skillId> <level>";
            break;
        }
        const uint16_t level = static_cast<uint16_t>(std::stoul(request.args[1]));
        auto& progress = player->characterState.skillOrDefault(request.args[0]);
        const uint16_t cap = skillResolver_.getCap(player->classId, request.args[0], player->level);
        progress.level = std::min(level, cap);
        progress.experience = 0;
        persistPlayerState(player->characterId, player->characterState);
        sendSkillsSnapshot(*player);
        response.ok = true;
        response.message = "Set " + request.args[0] + " to " + std::to_string(progress.level);
        break;
    }
    case net::DebugCommand::MaxSkills:
    {
        if (player == nullptr)
        {
            response.message = "Not in zone";
            break;
        }
        for (auto& [skillId, progress] : player->characterState.skills)
        {
            const uint16_t cap = skillResolver_.getCap(player->classId, skillId, player->level);
            if (cap > 0)
            {
                progress.level = cap;
                progress.experience = 0;
            }
        }
        persistPlayerState(player->characterId, player->characterState);
        sendSkillsSnapshot(*player);
        response.ok = true;
        response.message = "All trainable skills set to cap for level " + std::to_string(player->level);
        break;
    }
    case net::DebugCommand::GrantExperience:
    {
        if (player == nullptr || request.args.empty())
        {
            response.message = "Usage: grant_xp <amount>";
            break;
        }
        const uint32_t amount = static_cast<uint32_t>(std::stoul(request.args[0]));
        const auto levelResult = progression::grantCharacterExperience(player->characterState, amount);
        player->level = player->characterState.level;
        persistPlayerState(player->characterId, player->characterState);
        sendSkillsSnapshot(*player);
        response.ok = true;
        response.message = levelResult.leveledUp
            ? "Granted XP; leveled to " + std::to_string(levelResult.newLevel)
            : "Granted " + std::to_string(amount) + " XP";
        break;
    }
    case net::DebugCommand::PracticeSkill:
    {
        if (player == nullptr || request.args.empty())
        {
            response.message = "Usage: practice_skill <forage|pick_lock|meditate>";
            break;
        }
        const std::string& activity = request.args[0];
        if (activity == "forage")
        {
            applyActivitySkillGain(*player, "forage", "forage");
        }
        else if (activity == "pick_lock")
        {
            applyActivitySkillGain(*player, "pick_lock", "pick_lock");
        }
        else if (activity == "meditate")
        {
            applyActivitySkillGain(*player, "meditate", "meditate");
        }
        else
        {
            response.message = "Unknown activity: " + activity;
            break;
        }
        response.ok = true;
        response.message = "Practiced " + activity;
        break;
    }
    case net::DebugCommand::ListZones:
    {
        const auto zoneIds = database_.listZoneIds();
        std::string message;
        for (const auto& zoneId : zoneIds)
        {
            if (!message.empty())
            {
                message += ';';
            }
            const auto metadata = database_.loadZoneMetadata(zoneId);
            message += zoneId + ':' + (metadata.has_value() ? metadata->name : zoneId);
        }
        response.ok = true;
        response.message = message;
        break;
    }
    case net::DebugCommand::TeleportToZone:
    {
        if (player == nullptr || request.args.empty())
        {
            response.message = "Usage: teleport_to_zone <zoneId>";
            break;
        }

        const std::string& destZoneId = request.args[0];
        const auto metadata = database_.loadZoneMetadata(destZoneId);
        if (!metadata.has_value())
        {
            response.message = "Unknown zone: " + destZoneId;
            break;
        }

        const auto destination = findZoneCenterWalkable(destZoneId);
        if (!destination.has_value())
        {
            response.message = "No walkable tile found at center of " + destZoneId;
            break;
        }

        if (combatManager_ != nullptr)
        {
            combatManager_->forceEndCombat(player->characterId, combat::CombatOutcome::Fled);
        }

        if (destZoneId == config_.zoneId)
        {
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                pendingTransfers_.erase(player->characterId);
                player->tileX = destination->first;
                player->tileY = destination->second;
                player->locationDirty = true;

                for (auto& [_, ai] : aiCompanions_)
                {
                    if (ai.leaderCharacterId == player->characterId)
                    {
                        ai.tileX = player->tileX;
                        ai.tileY = player->tileY;
                    }
                }
            }

            persistPlayerToDatabase(*player);
            player->locationDirty = false;
            broadcastEntitySnapshot();
            response.ok = true;
            response.message = "Teleported to center of " + metadata->name;
            break;
        }

        std::string transferError;
        if (beginZoneTransfer(
                *player,
                connection,
                packet.header.sequenceId,
                destZoneId,
                destination->first,
                destination->second,
                transferError))
        {
            response.ok = true;
            response.message = "Transferring to " + metadata->name;
        }
        else
        {
            response.message = transferError;
        }
        break;
    }
    default:
    {
        const auto generic = debugHandler_.handle(request);
        response.ok = generic.ok;
        response.message = generic.message;
        break;
    }
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::DebugCommandResponse,
        packet.header.sequenceId,
        net::serialize(response),
        packet.header.sessionTokenHash);
}

void ZoneServer::deliverSystemMessage(const PlayerEntity& player, const std::string& text)
{
    if (!player.connected || player.connection == nullptr)
    {
        return;
    }

    net::ChatDeliverPayload deliver;
    deliver.channel = static_cast<uint8_t>(ChatChannel::System);
    deliver.senderName = "System";
    deliver.text = text;
    sendClientPacket(
        player.connection,
        net::ClientPacketType::ChatDeliver,
        0,
        net::serialize(deliver),
        player.sessionTokenHash);
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

std::optional<std::pair<int32_t, int32_t>> ZoneServer::findNearestWalkableTile(
    int32_t centerX,
    int32_t centerY) const
{
    return findNearestWalkableTileInGrid(zoneGrid_, centerX, centerY);
}

std::optional<std::pair<int32_t, int32_t>> ZoneServer::findZoneCenterWalkable(const std::string& zoneId) const
{
    world::ZoneGrid grid;
    if (!grid.loadFromDatabase(database_, zoneId, tileDefs_))
    {
        return std::nullopt;
    }

    const int32_t centerX = grid.width() / 2;
    const int32_t centerY = grid.height() / 2;
    return findNearestWalkableTileInGrid(grid, centerX, centerY);
}

bool ZoneServer::beginZoneTransfer(
    PlayerEntity& player,
    const std::shared_ptr<TcpConnection>& connection,
    uint32_t clientSequenceId,
    const std::string& destZoneId,
    int32_t destTileX,
    int32_t destTileY,
    std::string& outErrorMessage)
{
    if (!worldLink_)
    {
        outErrorMessage = "World link unavailable";
        return false;
    }

    persistPlayerToDatabase(player);
    player.locationDirty = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        pendingTransfers_[player.characterId] = PendingTransfer{
            player.characterId,
            player.sessionTokenHash,
            connection,
            clientSequenceId};
    }

    net::ZoneTransferRequestPayload transfer;
    transfer.characterId = player.characterId;
    transfer.sourceZoneId = config_.zoneId;
    transfer.destZoneId = destZoneId;
    transfer.destTileX = destTileX;
    transfer.destTileY = destTileY;
    transfer.sessionResumeToken = player.sessionTokenHash;

    sendServerPacketToWorld(
        worldLink_,
        net::ServerPacketType::ZoneTransferRequest,
        clientSequenceId,
        net::serialize(transfer));

    return true;
}

bool ZoneServer::executeStuckCommand(PlayerEntity& player, net::PlayerCommandResultPayload& result)
{
    if (combatManager_ != nullptr)
    {
        combatManager_->forceEndCombat(player.characterId, combat::CombatOutcome::Fled);
    }

    const int32_t centerX = zoneGrid_.width() / 2;
    const int32_t centerY = zoneGrid_.height() / 2;
    const auto destination = findNearestWalkableTile(centerX, centerY);
    if (!destination.has_value())
    {
        result.ok = false;
        result.message = "No safe location found in this zone.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        pendingTransfers_.erase(player.characterId);
        player.tileX = destination->first;
        player.tileY = destination->second;
        player.locationDirty = true;

        for (auto& [_, ai] : aiCompanions_)
        {
            if (ai.leaderCharacterId == player.characterId)
            {
                ai.tileX = player.tileX;
                ai.tileY = player.tileY;
            }
        }
    }

    persistPlayerToDatabase(player);
    player.locationDirty = false;
    broadcastEntitySnapshot();

    result.ok = true;
    result.message = "You have been relocated to zone center.";
    result.tileX = player.tileX;
    result.tileY = player.tileY;
    return true;
}

void ZoneServer::handlePlayerCommand(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::PlayerCommandRequestPayload request;
    net::PlayerCommandResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid player command";
        sendClientPacket(
            connection,
            net::ClientPacketType::PlayerCommandResult,
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
    else
    {
        std::string command = request.command;
        std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });

        if (command == "stuck")
        {
            executeStuckCommand(*player, result);
        }
        else
        {
            result.message = "Unknown command: " + request.command;
        }
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::PlayerCommandResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
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

    if (player->inCombat)
    {
        result.message = "Cannot use portal while in combat";
        sendClientPacket(
            connection,
            net::ClientPacketType::UsePortalResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    std::string transferError;
    if (!beginZoneTransfer(
            *player,
            connection,
            packet.header.sequenceId,
            portal->destZoneId,
            portal->destX,
            portal->destY,
            transferError))
    {
        result.message = transferError;
        sendClientPacket(
            connection,
            net::ClientPacketType::UsePortalResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

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