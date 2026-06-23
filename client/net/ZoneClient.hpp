#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>

#include <asio.hpp>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::client
{

class ZoneClient
{
public:
    using ChatCallback = std::function<void(const net::ChatDeliverPayload&)>;

    explicit ZoneClient(asio::io_context& io);

    bool connect(const std::string& host, uint16_t port);
    void close();
    bool isConnected() const;

    bool sessionResume(
        const std::string& characterId,
        uint64_t sessionTokenHash,
        net::ZoneSnapshotPayload& outSnapshot,
        bool fetchTiles = true);

    bool moveTo(int32_t tileX, int32_t tileY, net::MoveResultPayload& outResult);
    bool usePortal(net::ZoneConnectInfoPayload& outConnectInfo);
    bool sendSayChat(const std::string& text);

    std::optional<net::EntitySnapshotPayload> pollEntitySnapshot();
    void setChatCallback(ChatCallback callback);

    int32_t playerTileX() const { return playerTileX_; }
    int32_t playerTileY() const { return playerTileY_; }
    uint32_t playerEntityId() const { return playerEntityId_; }

private:
    bool sendPacket(net::ClientPacketType type, const net::ByteWriter& writer, uint64_t sessionTokenHash);
    bool readExact(std::size_t size, void* buffer, int timeoutMs);
    std::optional<net::SerializedPacket> readPacket(int timeoutMs);
    bool requestZoneTiles(net::ZoneSnapshotPayload& snapshot);

    asio::io_context& io_;
    asio::ip::tcp::socket socket_;
    uint32_t nextSequence_ = 1;
    uint64_t sessionTokenHash_ = 0;
    int32_t playerTileX_ = 0;
    int32_t playerTileY_ = 0;
    uint32_t playerEntityId_ = 0;
    ChatCallback chatCallback_;
    std::deque<net::EntitySnapshotPayload> pendingEntitySnapshots_;
};

} // namespace tbeq::client
