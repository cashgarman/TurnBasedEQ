#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>

#include <asio.hpp>

#include "tbeq/net/DebugCommands.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::client
{

class ZoneClient
{
public:
    using ChatCallback = std::function<void(const net::ChatDeliverPayload&)>;
    using CombatStartCallback = std::function<void(const net::CombatStartPayload&)>;
    using CombatUpdateCallback = std::function<void(const net::CombatUpdatePayload&)>;
    using CombatEventCallback = std::function<void(const net::CombatEventPayload&)>;
    using CombatEndCallback = std::function<void(const net::CombatEndPayload&)>;
    using VitalsCallback = std::function<void(const net::CharacterVitalsPayload&)>;
    using SkillGainCallback = std::function<void(const net::SkillGainPayload&)>;
    using LevelUpCallback = std::function<void(const net::LevelUpPayload&)>;
    using SkillsSnapshotCallback = std::function<void(const net::SkillsSnapshotPayload&)>;
    using InventorySnapshotCallback = std::function<void(const net::InventorySnapshotPayload&)>;
    using EquipItemResultCallback = std::function<void(const net::EquipItemResultPayload&)>;
    using UnequipItemResultCallback = std::function<void(const net::UnequipItemResultPayload&)>;
    using MerchantOpenCallback = std::function<void(const net::MerchantOpenPayload&)>;
    using MerchantBuyResultCallback = std::function<void(const net::MerchantBuyResultPayload&)>;
    using MerchantSellResultCallback = std::function<void(const net::MerchantSellResultPayload&)>;
    using NpcDialogOpenCallback = std::function<void(const net::NpcDialogOpenPayload&)>;

    explicit ZoneClient(asio::io_context& io);

    bool connect(const std::string& host, uint16_t port);
    void close();
    void disconnectGracefully();
    bool isConnected() const;

    bool sessionResume(
        const std::string& characterId,
        uint64_t sessionTokenHash,
        net::ZoneSnapshotPayload& outSnapshot,
        bool fetchTiles = true);

    bool moveTo(int32_t tileX, int32_t tileY, net::MoveResultPayload& outResult);
    bool usePortal(net::ZoneConnectInfoPayload& outConnectInfo, std::string* outErrorMessage = nullptr);
    bool sendSayChat(const std::string& text);
    bool sendPlayerCommand(
        const std::string& command,
        const std::vector<std::string>& args,
        net::PlayerCommandResultPayload& outResult);
    bool submitAction(const net::SubmitActionPayload& action, net::SubmitActionResultPayload& outResult);
    bool sendDebugCommand(net::DebugCommand command, const std::vector<std::string>& args, net::DebugCommandResponsePayload& outResponse);
    bool sendDebugTeleportToZone(
        const std::string& zoneId,
        net::DebugCommandResponsePayload& outResponse,
        std::optional<net::ZoneConnectInfoPayload>& outConnectInfo);
    bool equipItem(const std::string& itemId, net::EquipItemResultPayload& outResult);
    bool unequipItem(const std::string& slot, net::UnequipItemResultPayload& outResult);
    bool interactNpc(uint32_t npcEntityId);
    bool merchantBuy(uint32_t npcEntityId, const std::string& itemId, uint16_t quantity, net::MerchantBuyResultPayload& outResult);
    bool merchantSell(uint32_t npcEntityId, const std::string& itemId, uint16_t quantity, net::MerchantSellResultPayload& outResult);
    void pollGameplayPackets();

    std::optional<net::EntitySnapshotPayload> pollEntitySnapshot();
    void setChatCallback(ChatCallback callback);
    void setCombatStartCallback(CombatStartCallback callback);
    void setCombatUpdateCallback(CombatUpdateCallback callback);
    void setCombatEventCallback(CombatEventCallback callback);
    void setCombatEndCallback(CombatEndCallback callback);
    void setVitalsCallback(VitalsCallback callback);
    void setSkillGainCallback(SkillGainCallback callback);
    void setLevelUpCallback(LevelUpCallback callback);
    void setSkillsSnapshotCallback(SkillsSnapshotCallback callback);
    void setInventorySnapshotCallback(InventorySnapshotCallback callback);
    void setEquipItemResultCallback(EquipItemResultCallback callback);
    void setUnequipItemResultCallback(UnequipItemResultCallback callback);
    void setMerchantOpenCallback(MerchantOpenCallback callback);
    void setMerchantBuyResultCallback(MerchantBuyResultCallback callback);
    void setMerchantSellResultCallback(MerchantSellResultCallback callback);
    void setNpcDialogOpenCallback(NpcDialogOpenCallback callback);
    void setPlayerCommandResultCallback(std::function<void(const net::PlayerCommandResultPayload&)> callback);

    const net::InventorySnapshotPayload& inventorySnapshot() const { return inventorySnapshot_; }
    const net::SkillsSnapshotPayload& skillsSnapshot() const { return skillsSnapshot_; }

    int32_t playerTileX() const { return playerTileX_; }
    int32_t playerTileY() const { return playerTileY_; }
    uint32_t playerEntityId() const { return playerEntityId_; }

private:
    bool sendPacket(net::ClientPacketType type, const net::ByteWriter& writer, uint64_t sessionTokenHash);
    void syncPlayerPositionFromSnapshot(const net::EntitySnapshotPayload& snapshot);
    void queueEntitySnapshot(net::EntitySnapshotPayload snapshot);
    bool readExact(std::size_t size, void* buffer, int timeoutMs);
    std::optional<net::SerializedPacket> readPacket(int timeoutMs);
    bool requestZoneTiles(net::ZoneSnapshotPayload& snapshot);
    void dispatchGameplayPacket(const net::SerializedPacket& packet);

    asio::io_context& io_;
    asio::ip::tcp::socket socket_;
    uint32_t nextSequence_ = 1;
    uint64_t sessionTokenHash_ = 0;
    int32_t playerTileX_ = 0;
    int32_t playerTileY_ = 0;
    uint32_t playerEntityId_ = 0;
    ChatCallback chatCallback_;
    CombatStartCallback combatStartCallback_;
    CombatUpdateCallback combatUpdateCallback_;
    CombatEventCallback combatEventCallback_;
    CombatEndCallback combatEndCallback_;
    VitalsCallback vitalsCallback_;
    SkillGainCallback skillGainCallback_;
    LevelUpCallback levelUpCallback_;
    SkillsSnapshotCallback skillsSnapshotCallback_;
    InventorySnapshotCallback inventorySnapshotCallback_;
    EquipItemResultCallback equipItemResultCallback_;
    UnequipItemResultCallback unequipItemResultCallback_;
    MerchantOpenCallback merchantOpenCallback_;
    MerchantBuyResultCallback merchantBuyResultCallback_;
    MerchantSellResultCallback merchantSellResultCallback_;
    NpcDialogOpenCallback npcDialogOpenCallback_;
    std::function<void(const net::PlayerCommandResultPayload&)> playerCommandResultCallback_;
    net::InventorySnapshotPayload inventorySnapshot_;
    net::SkillsSnapshotPayload skillsSnapshot_;
    std::deque<net::EntitySnapshotPayload> pendingEntitySnapshots_;
};

} // namespace tbeq::client
