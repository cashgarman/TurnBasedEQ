#include "net/ZoneClient.hpp"

#include <chrono>
#include <cstring>
#include <thread>

#include <spdlog/spdlog.h>

#include "net/LoginProfiler.hpp"
#include "tbeq/social/ChatChannel.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::client
{

ZoneClient::ZoneClient(asio::io_context& io)
    : io_(io)
    , socket_(io)
{
}

bool ZoneClient::connect(const std::string& host, uint16_t port)
{
    try
    {
        asio::ip::tcp::resolver resolver(io_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        asio::connect(socket_, endpoints);
        std::error_code ec;
        socket_.non_blocking(true, ec);
        spdlog::info("[login] connected to zone {}:{}", host, port);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("[login] zone connect failed {}:{} ({})", host, port, ex.what());
        return false;
    }
}

void ZoneClient::close()
{
    std::error_code ec;
    socket_.close(ec);
}

bool ZoneClient::isConnected() const
{
    return socket_.is_open();
}

bool ZoneClient::sendPacket(net::ClientPacketType type, const net::ByteWriter& writer, uint64_t sessionTokenHash)
{
    auto packet = net::serializeClientPacket(type, nextSequence_++, writer);
    packet.header.sessionTokenHash = sessionTokenHash;
    const auto encoded = net::encodePacket(packet);
    std::error_code ec;
    asio::write(socket_, asio::buffer(encoded), ec);
    return !ec;
}

bool ZoneClient::readExact(std::size_t size, void* buffer, int timeoutMs)
{
    if (size == 0)
    {
        return true;
    }

    std::size_t received = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    auto* bytes = static_cast<uint8_t*>(buffer);

    while (received < size)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }

        std::error_code ec;
        const std::size_t read = socket_.read_some(
            asio::buffer(bytes + received, size - received),
            ec);
        if (ec == asio::error::would_block || ec == asio::error::try_again)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        if (ec)
        {
            return false;
        }
        if (read == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        received += read;
    }

    return true;
}

std::optional<net::SerializedPacket> ZoneClient::readPacket(int timeoutMs)
{
    net::PacketHeader header{};
    if (!readExact(sizeof(header), &header, timeoutMs) || !header.isValid())
    {
        return std::nullopt;
    }

    std::vector<uint8_t> payload(header.payloadLength);
    if (!payload.empty() && !readExact(payload.size(), payload.data(), timeoutMs))
    {
        return std::nullopt;
    }

    net::SerializedPacket packet;
    packet.header = header;
    packet.payload = std::move(payload);
    return packet;
}

bool ZoneClient::requestZoneTiles(net::ZoneSnapshotPayload& snapshot)
{
    spdlog::info("[login] requesting zone tile grid for {}", snapshot.zoneId);
    if (!sendPacket(
            net::ClientPacketType::RequestZoneTiles,
            net::serialize(net::RequestZoneTilesPayload{}),
            sessionTokenHash_))
    {
        spdlog::error("[login] failed to send RequestZoneTiles");
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0)
        {
            break;
        }

        const auto packet = readPacket(static_cast<int>(std::min<int64_t>(remainingMs, 2000)));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<net::ClientPacketType>(packet->header.packetType);
        if (type == net::ClientPacketType::ZoneTileGrid)
        {
            net::ZoneTileGridPayload grid;
            if (!net::deserializeClientPacket(*packet, grid))
            {
                spdlog::error("[login] failed to deserialize ZoneTileGrid");
                return false;
            }
            if (grid.zoneId != snapshot.zoneId)
            {
                spdlog::error("[login] ZoneTileGrid zone mismatch (got {}, expected {})", grid.zoneId, snapshot.zoneId);
                return false;
            }
            snapshot.tiles = std::move(grid.tiles);
            spdlog::info("[login] received zone tile grid ({} tiles)", snapshot.tiles.size());
            return true;
        }
        else if (type == net::ClientPacketType::EntitySnapshot)
        {
            net::EntitySnapshotPayload entities;
            if (net::deserializeClientPacket(*packet, entities))
            {
                pendingEntitySnapshots_.push_back(std::move(entities));
            }
        }
    }

    spdlog::error("[login] timed out waiting for ZoneTileGrid");
    return false;
}

bool ZoneClient::sessionResume(
    const std::string& characterId,
    uint64_t sessionTokenHash,
    net::ZoneSnapshotPayload& outSnapshot,
    bool fetchTiles)
{
    LoginProfiler profiler;
    sessionTokenHash_ = sessionTokenHash;

    spdlog::info("[login] session resume character={}", characterId);
    net::SessionResumePayload request;
    request.characterId = characterId;
    request.sessionResumeToken = sessionTokenHash;
    if (!sendPacket(net::ClientPacketType::SessionResume, net::serialize(request), sessionTokenHash))
    {
        spdlog::error("[login] failed to send SessionResume");
        return false;
    }

    bool gotResponse = false;
    bool gotSnapshot = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int packetIndex = 0;

    while ((!gotResponse || !gotSnapshot) && std::chrono::steady_clock::now() < deadline)
    {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0)
        {
            break;
        }

        LoginProfiler::Scope packetWait(
            profiler,
            "zone_read_packet_" + std::to_string(++packetIndex));
        const auto packet = readPacket(static_cast<int>(std::min<int64_t>(remainingMs, 2000)));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<net::ClientPacketType>(packet->header.packetType);
        spdlog::info(
            "[login] zone packet type={} payloadBytes={}",
            packet->header.packetType,
            packet->payload.size());
        if (type == net::ClientPacketType::SessionResumeResponse)
        {
            net::SessionResumeResponsePayload response;
            if (!net::deserializeClientPacket(*packet, response) || !response.ok)
            {
                spdlog::error("[login] SessionResumeResponse rejected: {}", response.message);
                return false;
            }
            playerEntityId_ = response.entityId;
            playerTileX_ = response.tileX;
            playerTileY_ = response.tileY;
            gotResponse = true;
            spdlog::info(
                "[login] session resumed entity={} at ({}, {})",
                playerEntityId_,
                playerTileX_,
                playerTileY_);
        }
        else if (type == net::ClientPacketType::ZoneSnapshot)
        {
            if (!net::deserializeClientPacket(*packet, outSnapshot))
            {
                spdlog::error("[login] failed to deserialize ZoneSnapshot");
                return false;
            }
            gotSnapshot = true;
            spdlog::info(
                "[login] zone metadata {} ({}x{})",
                outSnapshot.zoneId,
                outSnapshot.width,
                outSnapshot.height);
        }
        else if (type == net::ClientPacketType::EntitySnapshot)
        {
            net::EntitySnapshotPayload entities;
            if (net::deserializeClientPacket(*packet, entities))
            {
                pendingEntitySnapshots_.push_back(std::move(entities));
            }
        }
    }

    const auto drainDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < drainDeadline)
    {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            drainDeadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0)
        {
            break;
        }

        const auto packet = readPacket(static_cast<int>(std::min<int64_t>(remainingMs, 150)));
        if (!packet.has_value())
        {
            break;
        }

        const auto type = static_cast<net::ClientPacketType>(packet->header.packetType);
        if (type == net::ClientPacketType::EntitySnapshot)
        {
            net::EntitySnapshotPayload entities;
            if (net::deserializeClientPacket(*packet, entities))
            {
                pendingEntitySnapshots_.push_back(std::move(entities));
            }
        }
        else
        {
            spdlog::warn(
                "[login] unexpected packet type={} while draining entity snapshots after session resume",
                packet->header.packetType);
            break;
        }
    }

    if (!gotResponse || !gotSnapshot)
    {
        spdlog::error("[login] session resume incomplete (response={} snapshot={})", gotResponse, gotSnapshot);
        profiler.logSummary("zone_session_resume_failed");
        return false;
    }

    if (!fetchTiles || !outSnapshot.tiles.empty())
    {
        spdlog::info("[login] skipping tile grid fetch (fetchTiles={} cachedTiles={})",
            fetchTiles,
            outSnapshot.tiles.size());
        profiler.logSummary("zone_session_resume");
        return true;
    }

    LoginProfiler::Scope tileFetch(profiler, "zone_request_tiles");
    const bool tilesOk = requestZoneTiles(outSnapshot);
    profiler.logSummary(tilesOk ? "zone_session_resume" : "zone_session_resume_tiles_failed");
    return tilesOk;
}

bool ZoneClient::moveTo(int32_t tileX, int32_t tileY, net::MoveResultPayload& outResult)
{
    net::MoveIntentPayload intent;
    intent.targetTileX = tileX;
    intent.targetTileY = tileY;
    if (!sendPacket(net::ClientPacketType::MoveIntent, net::serialize(intent), sessionTokenHash_))
    {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0)
        {
            break;
        }

        const auto packet = readPacket(static_cast<int>(std::min<int64_t>(remainingMs, 2000)));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<net::ClientPacketType>(packet->header.packetType);
        if (type == net::ClientPacketType::MoveResult)
        {
            if (!net::deserializeClientPacket(*packet, outResult))
            {
                return false;
            }

            if (outResult.ok)
            {
                playerTileX_ = outResult.tileX;
                playerTileY_ = outResult.tileY;
                playerEntityId_ = outResult.entityId;
            }
            return outResult.ok;
        }
        if (type == net::ClientPacketType::EntitySnapshot)
        {
            net::EntitySnapshotPayload entities;
            if (net::deserializeClientPacket(*packet, entities))
            {
                pendingEntitySnapshots_.push_back(std::move(entities));
            }
        }
    }

    return false;
}

bool ZoneClient::usePortal(net::ZoneConnectInfoPayload& outConnectInfo)
{
    if (!sendPacket(net::ClientPacketType::UsePortal, net::serialize(net::UsePortalPayload{}), sessionTokenHash_))
    {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remainingMs <= 0)
        {
            break;
        }

        const auto packet = readPacket(static_cast<int>(std::min<int64_t>(remainingMs, 3000)));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<net::ClientPacketType>(packet->header.packetType);
        if (type == net::ClientPacketType::UsePortalResult)
        {
            net::UsePortalResultPayload result;
            if (!net::deserializeClientPacket(*packet, result) || !result.ok)
            {
                return false;
            }
        }
        else if (type == net::ClientPacketType::ZoneConnectInfo)
        {
            return net::deserializeClientPacket(*packet, outConnectInfo) && outConnectInfo.ok;
        }
    }

    return false;
}

bool ZoneClient::sendSayChat(const std::string& text)
{
    net::ChatMessagePayload message;
    message.channel = static_cast<uint8_t>(tbeq::ChatChannel::Say);
    message.text = text;
    return sendPacket(net::ClientPacketType::ChatMessage, net::serialize(message), sessionTokenHash_);
}

std::optional<net::EntitySnapshotPayload> ZoneClient::pollEntitySnapshot()
{
    if (!pendingEntitySnapshots_.empty())
    {
        auto snapshot = pendingEntitySnapshots_.front();
        pendingEntitySnapshots_.pop_front();
        return snapshot;
    }

    const auto packet = readPacket(100);
    if (!packet.has_value())
    {
        return std::nullopt;
    }

    if (static_cast<net::ClientPacketType>(packet->header.packetType) == net::ClientPacketType::EntitySnapshot)
    {
        net::EntitySnapshotPayload snapshot;
        if (net::deserializeClientPacket(*packet, snapshot))
        {
            return snapshot;
        }
    }
    else if (static_cast<net::ClientPacketType>(packet->header.packetType) == net::ClientPacketType::ChatDeliver)
    {
        net::ChatDeliverPayload deliver;
        if (net::deserializeClientPacket(*packet, deliver) && chatCallback_)
        {
            chatCallback_(deliver);
        }
    }

    return std::nullopt;
}

void ZoneClient::setChatCallback(ChatCallback callback)
{
    chatCallback_ = std::move(callback);
}

} // namespace tbeq::client
