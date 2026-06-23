#include "WorldLoginServer.hpp"

#include <chrono>
#include <fstream>
#include <limits>
#include <random>

#include <spdlog/spdlog.h>

#include "tbeq/auth/CharacterValidation.hpp"
#include "tbeq/auth/PasswordHash.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/net/ServerPackets.hpp"
#include "tbeq/worldgen/WorldBootstrap.hpp"

namespace tbeq::server
{

namespace
{

int64_t nowUnixSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

uint64_t fnv1a64(const std::string& value)
{
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : value)
    {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

WorldLoginServer::WorldLoginServer(asio::io_context& io, const AppConfig& config)
    : io_(io)
    , config_(config)
    , debugHandler_(config.devMode)
    , database_(config.dbPath)
    , zoneAcceptor_(io, config.worldLoginPort, [this](std::shared_ptr<TcpConnection> connection,
                                                      const net::SerializedPacket& packet)
    {
        handleZonePacket(std::move(connection), packet);
    })
    , clientAcceptor_(io, config.worldLoginClientPort, [this](std::shared_ptr<TcpConnection> connection,
                                                              const net::SerializedPacket& packet)
    {
        handleClientPacket(std::move(connection), packet);
    })
{
    zonePort_ = zoneAcceptor_.port();
    clientPort_ = clientAcceptor_.port();

    if (!bootstrapDatabase())
    {
        throw std::runtime_error("Failed to bootstrap database");
    }

    racesJson_ = loadJsonArray(std::filesystem::path(config.dataRoot) / "races.json");
    classesJson_ = loadJsonArray(std::filesystem::path(config.dataRoot) / "classes.json");

    spdlog::info("WorldLogin zone listener on port {}", zonePort_);
    spdlog::info("WorldLogin client listener on port {}", clientPort_);
    spdlog::info("WorldLogin database {}", config.dbPath);
    if (config.devMode)
    {
        spdlog::warn("WorldLogin running with --dev-mode ENABLED");
    }
}

bool WorldLoginServer::bootstrapDatabase()
{
    if (!database_.open())
    {
        return false;
    }
    if (!database_.initializeSchema())
    {
        return false;
    }
    return ensureWorldGenerated();
}

bool WorldLoginServer::ensureWorldGenerated()
{
    return worldgen::ensureWorldInDatabase(
        database_,
        config_.worldSeed,
        std::filesystem::path(config_.dataRoot));
}

nlohmann::json WorldLoginServer::loadJsonArray(const std::filesystem::path& path) const
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        spdlog::warn("Missing JSON file {}", path.string());
        return nlohmann::json::array();
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_array())
    {
        spdlog::warn("Expected JSON array in {}", path.string());
        return nlohmann::json::array();
    }
    return json;
}

uint64_t WorldLoginServer::sessionTokenHash(uint64_t token) const
{
    return fnv1a64(std::to_string(token));
}

uint64_t WorldLoginServer::generateTokenValue()
{
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<uint64_t> distribution(1, std::numeric_limits<uint64_t>::max());
    return distribution(generator);
}

uint64_t WorldLoginServer::issueSession(int64_t accountId)
{
    database_.deleteExpiredSessions(nowUnixSeconds());

    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const uint64_t token = generateTokenValue();
        const uint64_t hash = sessionTokenHash(token);
        const int64_t expiresAt = nowUnixSeconds() + 60 * 60 * 24;
        if (database_.createSession(hash, accountId, expiresAt))
        {
            return token;
        }
    }

    return 0;
}

std::optional<int64_t> WorldLoginServer::validateSession(uint64_t tokenHash) const
{
    if (tokenHash == 0)
    {
        return std::nullopt;
    }

    // Session lookup requires the raw token; clients send hash in packet header.
    // For Phase 1 we treat the header hash as the session key by storing hash in sessions.token.
    const auto session = database_.findSession(tokenHash);
    if (!session.has_value())
    {
        return std::nullopt;
    }

    if (session->expiresAt <= nowUnixSeconds())
    {
        return std::nullopt;
    }

    return session->accountId;
}

void WorldLoginServer::sendClientPacket(
    const std::shared_ptr<TcpConnection>& connection,
    net::ClientPacketType type,
    uint32_t sequenceId,
    const net::ByteWriter& writer,
    uint64_t sessionTokenHashValue) const
{
    auto packet = net::serializeClientPacket(type, sequenceId, writer);
    packet.header.sessionTokenHash = sessionTokenHashValue;
    if (connection)
    {
        connection->send(packet);
    }
}

void WorldLoginServer::sendServerPacketToZone(
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

std::shared_ptr<TcpConnection> WorldLoginServer::findZoneConnection(const std::string& zoneId) const
{
    std::lock_guard<std::mutex> lock(zonesMutex_);
    const auto it = zones_.find(zoneId);
    if (it == zones_.end())
    {
        return nullptr;
    }
    return it->second.connection;
}

void WorldLoginServer::handleZonePacket(std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet)
{
    const auto type = static_cast<net::ServerPacketType>(packet.header.packetType);
    switch (type)
    {
    case net::ServerPacketType::ZoneRegister:
    {
        net::ZoneRegisterPayload payload;
        if (!net::deserializeServerPacket(packet, payload))
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

        net::ZoneRegisterAckPayload ack;
        ack.accepted = true;
        ack.message = "registered";
        sendServerPacketToZone(connection, net::ServerPacketType::ZoneRegisterAck, packet.header.sequenceId, net::serialize(ack));
        break;
    }
    case net::ServerPacketType::Ping:
    {
        net::ServerPingPayload ping;
        if (!net::deserializeServerPacket(packet, ping))
        {
            return;
        }
        net::ServerPongPayload pong;
        pong.timestampMs = ping.timestampMs;
        sendServerPacketToZone(connection, net::ServerPacketType::Pong, packet.header.sequenceId, net::serialize(pong));
        break;
    }
    case net::ServerPacketType::PlayerEnterReady:
    {
        net::PlayerEnterReadyPayload ready;
        if (!net::deserializeServerPacket(packet, ready))
        {
            return;
        }

        std::optional<PendingZoneTransfer> transfer;
        {
            std::lock_guard<std::mutex> lock(transferMutex_);
            const auto it = pendingZoneTransfers_.find(ready.characterId);
            if (it != pendingZoneTransfers_.end())
            {
                transfer = it->second;
                pendingZoneTransfers_.erase(it);
            }
        }

        if (transfer.has_value())
        {
            const auto sourceConnection = findZoneConnection(transfer->sourceZoneId);
            if (sourceConnection != nullptr)
            {
                net::ZoneTransferAuthorizePayload authorize;
                authorize.characterId = ready.characterId;
                sendServerPacketToZone(
                    sourceConnection,
                    net::ServerPacketType::ZoneTransferAuthorize,
                    packet.header.sequenceId,
                    net::serialize(authorize));
            }

            net::ZoneConnectForwardPayload forward;
            forward.characterId = ready.characterId;
            if (!ready.ok)
            {
                forward.ok = false;
                forward.message = ready.message;
            }
            else
            {
                std::lock_guard<std::mutex> lock(zonesMutex_);
                const auto it = zones_.find(transfer->destZoneId);
                if (it == zones_.end())
                {
                    forward.ok = false;
                    forward.message = "Destination zone unavailable";
                }
                else
                {
                    forward.ok = true;
                    forward.message = "Zone transfer";
                    forward.zoneId = transfer->destZoneId;
                    forward.zoneHost = it->second.payload.listenHost;
                    forward.zonePort = it->second.payload.listenPort;
                    forward.sessionResumeToken = transfer->sessionTokenHash;
                }
            }

            if (sourceConnection != nullptr)
            {
                sendServerPacketToZone(
                    sourceConnection,
                    net::ServerPacketType::ZoneConnectForward,
                    packet.header.sequenceId,
                    net::serialize(forward));
            }
            break;
        }

        PendingHandoff handoff;
        {
            std::lock_guard<std::mutex> lock(handoffMutex_);
            const auto it = pendingHandoffs_.find(ready.characterId);
            if (it == pendingHandoffs_.end())
            {
                spdlog::warn("PlayerEnterReady for unknown character {}", ready.characterId);
                return;
            }
            handoff = std::move(it->second);
            pendingHandoffs_.erase(it);
        }

        net::ZoneConnectInfoPayload connectInfo;
        connectInfo.characterId = handoff.characterId;
        if (!ready.ok)
        {
            connectInfo.ok = false;
            connectInfo.message = ready.message;
        }
        else
        {
            const auto character = database_.findCharacter(ready.characterId);
            if (!character.has_value())
            {
                connectInfo.ok = false;
                connectInfo.message = "Character not found";
            }
            else
            {
                const auto zoneConnection = findZoneConnection(character->zoneId);
                std::lock_guard<std::mutex> lock(zonesMutex_);
                const auto it = zones_.find(character->zoneId);
                if (zoneConnection == nullptr || it == zones_.end())
                {
                    connectInfo.ok = false;
                    connectInfo.message = "Zone server unavailable";
                }
                else
                {
                    connectInfo.ok = true;
                    connectInfo.message = "Enter world";
                    connectInfo.zoneId = character->zoneId;
                    connectInfo.zoneHost = it->second.payload.listenHost;
                    connectInfo.zonePort = it->second.payload.listenPort;
                    connectInfo.sessionResumeToken = handoff.sessionTokenHash;
                }
            }
        }

        sendClientPacket(
            handoff.clientConnection,
            net::ClientPacketType::ZoneConnectInfo,
            handoff.clientSequenceId,
            net::serialize(connectInfo),
            handoff.sessionTokenHash);
        break;
    }
    case net::ServerPacketType::LoadCharacterRequest:
    {
        net::LoadCharacterRequestPayload request;
        if (!net::deserializeServerPacket(packet, request))
        {
            return;
        }

        net::LoadCharacterResponsePayload response;
        const auto character = database_.findCharacter(request.characterId);
        if (!character.has_value())
        {
            response.ok = false;
            response.message = "Character not found";
        }
        else
        {
            response.ok = true;
            response.message = "ok";
            response.characterId = character->id;
            response.name = character->name;
            response.raceId = character->raceId;
            response.classId = character->classId;
            response.level = character->level;
            response.zoneId = character->zoneId;
            response.posX = character->posX;
            response.posY = character->posY;
            response.stateJson = character->stateJson;
        }

        sendServerPacketToZone(
            connection,
            net::ServerPacketType::LoadCharacterResponse,
            packet.header.sequenceId,
            net::serialize(response));
        break;
    }
    case net::ServerPacketType::ZoneTransferRequest:
    {
        net::ZoneTransferRequestPayload request;
        if (!net::deserializeServerPacket(packet, request))
        {
            return;
        }

        spdlog::info(
            "ZoneTransferRequest character={} {} -> {}",
            request.characterId,
            request.sourceZoneId,
            request.destZoneId);

        const auto character = database_.findCharacter(request.characterId);
        if (!character.has_value())
        {
            spdlog::warn("Zone transfer for unknown character {}", request.characterId);
            const auto sourceConnection = findZoneConnection(request.sourceZoneId);
            if (sourceConnection != nullptr)
            {
                net::ZoneConnectForwardPayload forward;
                forward.ok = false;
                forward.message = "Character not found";
                forward.characterId = request.characterId;
                sendServerPacketToZone(
                    sourceConnection,
                    net::ServerPacketType::ZoneConnectForward,
                    packet.header.sequenceId,
                    net::serialize(forward));
            }
            return;
        }

        database_.updateCharacterLocation(
            request.characterId,
            request.destZoneId,
            static_cast<float>(request.destTileX),
            static_cast<float>(request.destTileY));

        const auto destConnection = findZoneConnection(request.destZoneId);
        if (destConnection == nullptr)
        {
            spdlog::warn("Destination zone {} unavailable for transfer", request.destZoneId);
            const auto sourceConnection = findZoneConnection(request.sourceZoneId);
            if (sourceConnection != nullptr)
            {
                net::ZoneConnectForwardPayload forward;
                forward.ok = false;
                forward.message = "Destination zone unavailable";
                forward.characterId = request.characterId;
                sendServerPacketToZone(
                    sourceConnection,
                    net::ServerPacketType::ZoneConnectForward,
                    packet.header.sequenceId,
                    net::serialize(forward));
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(transferMutex_);
            pendingZoneTransfers_[request.characterId] = PendingZoneTransfer{
                request.sourceZoneId,
                request.destZoneId,
                request.sessionResumeToken,
                request.destTileX,
                request.destTileY};
        }

        net::PlayerEnterPreparePayload prepare;
        prepare.characterId = request.characterId;
        prepare.accountId = character->accountId;
        prepare.zoneId = request.destZoneId;
        prepare.sessionResumeToken = request.sessionResumeToken;
        sendServerPacketToZone(
            destConnection,
            net::ServerPacketType::PlayerEnterPrepare,
            packet.header.sequenceId,
            net::serialize(prepare));
        break;
    }
    case net::ServerPacketType::PlayerDisconnect:
    {
        net::PlayerDisconnectPayload disconnectPayload;
        if (!net::deserializeServerPacket(packet, disconnectPayload))
        {
            return;
        }
        spdlog::info("PlayerDisconnect character={}", disconnectPayload.characterId);
        break;
    }
    case net::ServerPacketType::ZoneTransferComplete:
    {
        net::ZoneTransferCompletePayload completePayload;
        if (!net::deserializeServerPacket(packet, completePayload))
        {
            return;
        }
        spdlog::info(
            "ZoneTransferComplete character={} zone={} pos=({}, {})",
            completePayload.characterId,
            completePayload.zoneId,
            completePayload.posX,
            completePayload.posY);
        break;
    }
    case net::ServerPacketType::DebugCommandRequest:
    {
        net::ServerDebugCommandRequestPayload request;
        if (!net::deserializeServerPacket(packet, request))
        {
            return;
        }
        const auto result = debugHandler_.handle(request);
        net::ServerDebugCommandResponsePayload responsePayload;
        responsePayload.ok = result.ok;
        responsePayload.message = result.message;
        sendServerPacketToZone(
            connection,
            net::ServerPacketType::DebugCommandResponse,
            packet.header.sequenceId,
            net::serialize(responsePayload));
        break;
    }
    default:
        spdlog::warn("Unhandled server packet type {}", packet.header.packetType);
        break;
    }
}

void WorldLoginServer::handleClientPacket(std::shared_ptr<TcpConnection> connection, const net::SerializedPacket& packet)
{
    const auto type = static_cast<net::ClientPacketType>(packet.header.packetType);
    switch (type)
    {
    case net::ClientPacketType::CreateAccountRequest:
    {
        net::CreateAccountRequestPayload request;
        if (!net::deserializeClientPacket(packet, request))
        {
            return;
        }

        net::CreateAccountResponsePayload response;
        const auto usernameResult = auth::validateUsername(request.username);
        const auto passwordResult = auth::validatePassword(request.password);
        if (!usernameResult.ok)
        {
            response.message = usernameResult.message;
        }
        else if (!passwordResult.ok)
        {
            response.message = passwordResult.message;
        }
        else if (database_.findAccountByUsername(request.username).has_value())
        {
            response.message = "Username already exists";
        }
        else
        {
            const auto hashed = auth::hashPassword(request.password);
            if (database_.createAccount(request.username, hashed.hash, hashed.salt, config_.devMode))
            {
                response.ok = true;
                response.message = "Account created";
            }
            else
            {
                response.message = "Failed to create account";
            }
        }

        sendClientPacket(connection, net::ClientPacketType::CreateAccountResponse, packet.header.sequenceId, net::serialize(response));
        break;
    }
    case net::ClientPacketType::LoginRequest:
    {
        net::LoginRequestPayload request;
        if (!net::deserializeClientPacket(packet, request))
        {
            return;
        }

        net::LoginResponsePayload response;
        const auto account = database_.findAccountByUsername(request.username);
        if (!account.has_value())
        {
            response.message = "Invalid credentials";
        }
        else if (account->isBanned)
        {
            response.message = "Account banned";
        }
        else if (!auth::verifyPassword(request.password, account->passwordHash, account->passwordSalt))
        {
            response.message = "Invalid credentials";
        }
        else
        {
            const uint64_t token = issueSession(account->id);
            if (token == 0)
            {
                response.message = "Failed to create session";
            }
            else
            {
                response.ok = true;
                response.message = "Login successful";
                response.sessionToken = token;
                response.sessionTokenHash = sessionTokenHash(token);
            }
        }

        sendClientPacket(
            connection,
            net::ClientPacketType::LoginResponse,
            packet.header.sequenceId,
            net::serialize(response),
            response.sessionTokenHash);
        break;
    }
    case net::ClientPacketType::CharacterListRequest:
    {
        net::CharacterListResponsePayload response;
        const auto accountId = validateSession(packet.header.sessionTokenHash);
        if (!accountId.has_value())
        {
            response.message = "Invalid session";
        }
        else
        {
            response.ok = true;
            response.message = "ok";
            for (const auto& character : database_.listCharacters(*accountId))
            {
                net::CharacterSummaryPayload summary;
                summary.characterId = character.id;
                summary.name = character.name;
                summary.raceId = character.raceId;
                summary.classId = character.classId;
                summary.level = character.level;
                summary.zoneId = character.zoneId;
                response.characters.push_back(std::move(summary));
            }
        }

        sendClientPacket(
            connection,
            net::ClientPacketType::CharacterListResponse,
            packet.header.sequenceId,
            net::serialize(response),
            packet.header.sessionTokenHash);
        break;
    }
    case net::ClientPacketType::CreateCharacterRequest:
    {
        net::CreateCharacterRequestPayload request;
        if (!net::deserializeClientPacket(packet, request))
        {
            return;
        }

        net::CreateCharacterResponsePayload response;
        const auto accountId = validateSession(packet.header.sessionTokenHash);
        if (!accountId.has_value())
        {
            response.message = "Invalid session";
        }
        else if (database_.characterCountForAccount(*accountId) >= 8)
        {
            response.message = "Character limit reached";
        }
        else
        {
            const auto validation = auth::validateCharacterCreate(
                request.name,
                request.raceId,
                request.classId,
                racesJson_,
                classesJson_);
            if (!validation.ok)
            {
                response.message = validation.message;
            }
            else
            {
                std::string startZone = "starter_city";
                for (const auto& race : racesJson_)
                {
                    if (race.contains("id") && race["id"].get<std::string>() == request.raceId && race.contains("startZone"))
                    {
                        startZone = race["startZone"].get<std::string>();
                        break;
                    }
                }

                const std::string characterId = database_.createCharacter(
                    *accountId,
                    request.name,
                    request.raceId,
                    request.classId,
                    startZone);
                if (!characterId.empty())
                {
                    response.ok = true;
                    response.message = "Character created";
                    response.characterId = characterId;
                }
                else
                {
                    for (const auto& character : database_.listCharacters(*accountId))
                    {
                        if (character.name == request.name)
                        {
                            response.ok = true;
                            response.message = "Character already exists";
                            response.characterId = character.id;
                            break;
                        }
                    }

                    if (!response.ok)
                    {
                        response.message = "Failed to create character (name may already exist)";
                    }
                }
            }
        }

        sendClientPacket(
            connection,
            net::ClientPacketType::CreateCharacterResponse,
            packet.header.sequenceId,
            net::serialize(response),
            packet.header.sessionTokenHash);
        break;
    }
    case net::ClientPacketType::SelectCharacterRequest:
    {
        net::SelectCharacterRequestPayload request;
        if (!net::deserializeClientPacket(packet, request))
        {
            return;
        }

        const auto accountId = validateSession(packet.header.sessionTokenHash);
        if (!accountId.has_value())
        {
            net::ZoneConnectInfoPayload connectInfo;
            connectInfo.ok = false;
            connectInfo.message = "Invalid session";
            sendClientPacket(
                connection,
                net::ClientPacketType::ZoneConnectInfo,
                packet.header.sequenceId,
                net::serialize(connectInfo));
            break;
        }

        const auto character = database_.findCharacterForAccount(*accountId, request.characterId);
        if (!character.has_value())
        {
            net::ZoneConnectInfoPayload connectInfo;
            connectInfo.ok = false;
            connectInfo.message = "Character not found";
            sendClientPacket(
                connection,
                net::ClientPacketType::ZoneConnectInfo,
                packet.header.sequenceId,
                net::serialize(connectInfo),
                packet.header.sessionTokenHash);
            break;
        }

        const auto zoneConnection = findZoneConnection(character->zoneId);
        if (zoneConnection == nullptr)
        {
            net::ZoneConnectInfoPayload connectInfo;
            connectInfo.ok = false;
            connectInfo.message = "Zone server unavailable";
            sendClientPacket(
                connection,
                net::ClientPacketType::ZoneConnectInfo,
                packet.header.sequenceId,
                net::serialize(connectInfo),
                packet.header.sessionTokenHash);
            break;
        }

        const uint64_t resumeToken = packet.header.sessionTokenHash;
        {
            std::lock_guard<std::mutex> lock(handoffMutex_);
            pendingHandoffs_[character->id] = PendingHandoff{
                connection,
                packet.header.sequenceId,
                packet.header.sessionTokenHash,
                character->id};
        }

        net::PlayerEnterPreparePayload prepare;
        prepare.characterId = character->id;
        prepare.accountId = character->accountId;
        prepare.zoneId = character->zoneId;
        prepare.sessionResumeToken = resumeToken;
        sendServerPacketToZone(
            zoneConnection,
            net::ServerPacketType::PlayerEnterPrepare,
            packet.header.sequenceId,
            net::serialize(prepare));
        break;
    }
    case net::ClientPacketType::Ping:
    {
        net::ClientPingPayload ping;
        if (!net::deserializeClientPacket(packet, ping))
        {
            return;
        }
        net::ClientPongPayload pong;
        pong.timestampMs = ping.timestampMs;
        sendClientPacket(
            connection,
            net::ClientPacketType::Pong,
            packet.header.sequenceId,
            net::serialize(pong),
            packet.header.sessionTokenHash);
        break;
    }
    default:
        spdlog::warn("Unhandled client packet type {}", packet.header.packetType);
        break;
    }
}

} // namespace tbeq::server
