#include "ZoneServer.hpp"

#include <cmath>

#include <algorithm>
#include <spdlog/spdlog.h>

#include "tbeq/content/ItemCatalog.hpp"
#include "tbeq/content/NpcCatalog.hpp"
#include "tbeq/items/ItemRules.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::server
{

namespace
{

constexpr int32_t kNpcInteractRadius = 3;

int32_t manhattanDistance(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return std::abs(x0 - x1) + std::abs(y0 - y1);
}

bool npcHasRole(const content::NpcDef& npcDef, const std::string& role)
{
    return std::find(npcDef.roles.begin(), npcDef.roles.end(), role) != npcDef.roles.end();
}

} // namespace

uint16_t ZoneServer::merchantStockQuantity(
    const NpcEntity& npc,
    const std::string& itemId,
    uint16_t catalogQuantity) const
{
    const auto it = npc.merchantStockRemaining.find(itemId);
    if (it != npc.merchantStockRemaining.end())
    {
        return it->second;
    }
    return catalogQuantity;
}

net::MerchantOpenPayload ZoneServer::buildMerchantOpenPayload(
    const NpcEntity& npc,
    const content::NpcDef& npcDef,
    const CharacterState& characterState) const
{
    net::MerchantOpenPayload openPayload;
    openPayload.npcEntityId = npc.entityId;
    openPayload.npcName = npcDef.name;
    openPayload.sellItemIds = npcDef.merchantSellItemIds;

    const uint16_t merchantSkill = characterState.skillLevel("merchant");
    for (const auto& buyEntry : npcDef.merchantBuyStock)
    {
        const content::ItemDef* item = itemCatalog_.findItem(buyEntry.itemId);
        if (item == nullptr)
        {
            continue;
        }

        net::MerchantStockEntryPayload stockEntry;
        stockEntry.itemId = buyEntry.itemId;
        stockEntry.quantity = merchantStockQuantity(npc, buyEntry.itemId, buyEntry.quantity);
        stockEntry.buyPrice = items::ItemRules::merchantBuyPrice(
            *item,
            merchantSkill,
            characterState.cha,
            buyEntry.price);
        stockEntry.sellPrice = items::ItemRules::merchantSellPrice(
            *item,
            merchantSkill,
            characterState.cha);
        openPayload.stock.push_back(std::move(stockEntry));
    }

    return openPayload;
}

net::InventorySnapshotPayload ZoneServer::buildInventorySnapshot(const CharacterState& state) const
{
    net::InventorySnapshotPayload payload;
    payload.gold = state.gold;
    for (const auto& item : state.inventory)
    {
        net::InventoryEntryPayload entry;
        entry.itemId = item.itemId;
        entry.quantity = item.quantity;
        payload.items.push_back(std::move(entry));
    }
    for (const auto& [slot, itemId] : state.equipment)
    {
        net::EquipmentEntryPayload entry;
        entry.slot = slot;
        entry.itemId = itemId;
        payload.equipment.push_back(std::move(entry));
    }
    return payload;
}

void ZoneServer::sendInventorySnapshot(const PlayerEntity& player)
{
    if (!player.connected || player.connection == nullptr)
    {
        return;
    }

    const auto payload = buildInventorySnapshot(player.characterState);
    sendClientPacket(
        player.connection,
        net::ClientPacketType::InventorySnapshot,
        0,
        net::serialize(payload),
        player.sessionTokenHash);
}

ZoneServer::NpcEntity* ZoneServer::findNpcByEntityId(uint32_t entityId)
{
    for (auto& npc : npcs_)
    {
        if (npc.entityId == entityId)
        {
            return &npc;
        }
    }
    return nullptr;
}

bool ZoneServer::isNearNpc(const PlayerEntity& player, const NpcEntity& npc) const
{
    return manhattanDistance(player.tileX, player.tileY, npc.tileX, npc.tileY) <= kNpcInteractRadius;
}

bool ZoneServer::tryEquipItem(PlayerEntity& player, const std::string& itemId, std::string& message)
{
    if (player.inCombat)
    {
        message = "Cannot change equipment during combat.";
        return false;
    }

    const content::ItemDef* item = itemCatalog_.findItem(itemId);
    if (item == nullptr)
    {
        message = "Unknown item.";
        return false;
    }
    if (item->slot == content::ItemSlot::None)
    {
        message = "Item is not equippable.";
        return false;
    }
    if (player.characterState.itemQuantity(itemId) == 0)
    {
        message = "Item not in inventory.";
        return false;
    }

    items::EquipContext context;
    context.classId = player.classId;
    context.raceId = player.raceId;
    context.level = player.level;
    const auto check = items::ItemRules::canEquip(*item, context, player.characterState);
    if (!check.ok)
    {
        message = check.message;
        return false;
    }

    const std::string slot = content::itemSlotToString(item->slot);
    const std::string previousItemId = player.characterState.equippedItemInSlot(slot);
    if (!player.characterState.removeItem(itemId, 1))
    {
        message = "Failed to remove item from inventory.";
        return false;
    }
    if (!previousItemId.empty())
    {
        player.characterState.addItem(previousItemId, 1);
    }

    player.characterState.equipment[slot] = itemId;
    items::ItemRules::recomputeDerivedStats(player.characterState, itemCatalog_);
    persistPlayerState(player.characterId, player.characterState);
    sendInventorySnapshot(player);

    net::CharacterVitalsPayload vitals;
    vitals.hp = player.characterState.hp;
    vitals.maxHp = player.characterState.maxHp;
    vitals.mana = player.characterState.mana;
    vitals.maxMana = player.characterState.maxMana;
    broadcastToPlayers({player.characterId}, net::ClientPacketType::CharacterVitals, net::serialize(vitals));
    broadcastEntitySnapshot();

    message = "Equipped " + item->name + ".";
    return true;
}

bool ZoneServer::tryUnequipItem(PlayerEntity& player, const std::string& slot, std::string& message)
{
    if (player.inCombat)
    {
        message = "Cannot change equipment during combat.";
        return false;
    }

    const auto it = player.characterState.equipment.find(slot);
    if (it == player.characterState.equipment.end())
    {
        message = "Nothing equipped in that slot.";
        return false;
    }

    player.characterState.addItem(it->second, 1);
    player.characterState.equipment.erase(it);
    items::ItemRules::recomputeDerivedStats(player.characterState, itemCatalog_);
    persistPlayerState(player.characterId, player.characterState);
    sendInventorySnapshot(player);

    net::CharacterVitalsPayload vitals;
    vitals.hp = player.characterState.hp;
    vitals.maxHp = player.characterState.maxHp;
    vitals.mana = player.characterState.mana;
    vitals.maxMana = player.characterState.maxMana;
    broadcastToPlayers({player.characterId}, net::ClientPacketType::CharacterVitals, net::serialize(vitals));
    broadcastEntitySnapshot();

    message = "Unequipped item.";
    return true;
}

void ZoneServer::handleEquipItem(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::EquipItemRequestPayload request;
    net::EquipItemResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid equip request.";
        sendClientPacket(
            connection,
            net::ClientPacketType::EquipItemResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr)
    {
        result.message = "Not in zone.";
    }
    else
    {
        result.ok = tryEquipItem(*player, request.itemId, result.message);
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::EquipItemResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleUnequipItem(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::UnequipItemRequestPayload request;
    net::UnequipItemResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid unequip request.";
        sendClientPacket(
            connection,
            net::ClientPacketType::UnequipItemResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr)
    {
        result.message = "Not in zone.";
    }
    else
    {
        result.ok = tryUnequipItem(*player, request.slot, result.message);
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::UnequipItemResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleNpcInteract(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::NpcInteractRequestPayload request;
    if (!net::deserializeClientPacket(packet, request))
    {
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    if (player == nullptr)
    {
        return;
    }

    NpcEntity* npc = findNpcByEntityId(request.npcEntityId);
    if (npc == nullptr)
    {
        deliverSystemMessage(*player, "That NPC is not here.");
        return;
    }
    if (!isNearNpc(*player, *npc))
    {
        deliverSystemMessage(*player, "Move closer to interact with that NPC.");
        return;
    }

    const std::string resolvedNpcId = npcCatalog_.resolveNpcId(npc->npcId);
    const content::NpcDef* npcDef = npcCatalog_.findNpc(resolvedNpcId);
    if (npcDef == nullptr)
    {
        spdlog::warn(
            "NpcInteract failed: unknown npc id '{}' (resolved '{}')",
            npc->npcId,
            resolvedNpcId);
        deliverSystemMessage(*player, "That NPC cannot be interacted with yet.");
        return;
    }

    const bool isLorekeeper = npcHasRole(*npcDef, "lorekeeper");
    const bool isMerchant = npcHasRole(*npcDef, "merchant");
    if (isLorekeeper)
    {
        net::NpcDialogOpenPayload dialogPayload;
        dialogPayload.npcEntityId = npc->entityId;
        dialogPayload.npcName = npcDef->name;
        if (npcDef->loreLines.empty())
        {
            dialogPayload.lines.push_back(npcDef->name + " has no tales to share yet.");
        }
        else
        {
            dialogPayload.lines = npcDef->loreLines;
        }

        sendClientPacket(
            connection,
            net::ClientPacketType::NpcDialogOpen,
            packet.header.sequenceId,
            net::serialize(dialogPayload),
            packet.header.sessionTokenHash);
        return;
    }

    if (!isMerchant)
    {
        deliverSystemMessage(*player, npcDef->name + " has nothing to offer right now.");
        return;
    }

    const auto openPayload = buildMerchantOpenPayload(*npc, *npcDef, player->characterState);

    sendClientPacket(
        connection,
        net::ClientPacketType::MerchantOpen,
        packet.header.sequenceId,
        net::serialize(openPayload),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleMerchantBuy(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::MerchantBuyRequestPayload request;
    net::MerchantBuyResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid buy request.";
        sendClientPacket(
            connection,
            net::ClientPacketType::MerchantBuyResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    NpcEntity* npc = findNpcByEntityId(request.npcEntityId);
    const content::ItemDef* item = itemCatalog_.findItem(request.itemId);
    const content::NpcDef* npcDef = npc != nullptr ? npcCatalog_.findNpc(npc->npcId) : nullptr;

    if (player == nullptr || npc == nullptr || item == nullptr || npcDef == nullptr)
    {
        result.message = "Invalid merchant transaction.";
    }
    else if (!isNearNpc(*player, *npc))
    {
        result.message = "Too far from merchant.";
    }
    else if (request.quantity == 0)
    {
        result.message = "Invalid quantity.";
    }
    else
    {
        const content::MerchantBuyEntry* stockEntry = nullptr;
        for (const auto& entry : npcDef->merchantBuyStock)
        {
            if (entry.itemId == request.itemId)
            {
                stockEntry = &entry;
                break;
            }
        }

        if (stockEntry == nullptr)
        {
            result.message = "Merchant does not sell that item.";
        }
        else
        {
            const uint16_t merchantSkill = player->characterState.skillLevel("merchant");
            const uint32_t unitPrice = items::ItemRules::merchantBuyPrice(
                *item,
                merchantSkill,
                player->characterState.cha,
                stockEntry->price);
            const uint32_t totalPrice = unitPrice * request.quantity;
            const uint16_t available = merchantStockQuantity(*npc, request.itemId, stockEntry->quantity);
            if (available < request.quantity)
            {
                result.message = "Merchant is out of stock.";
            }
            else if (player->characterState.gold < totalPrice)
            {
                result.message = "Not enough gold.";
            }
            else
            {
                auto stockIt = npc->merchantStockRemaining.find(request.itemId);
                if (stockIt != npc->merchantStockRemaining.end())
                {
                    stockIt->second = static_cast<uint16_t>(stockIt->second - request.quantity);
                    result.stockUpdated = true;
                    result.stockItemId = request.itemId;
                    result.stockQuantity = stockIt->second;
                }

                player->characterState.gold -= totalPrice;
                player->characterState.addItem(request.itemId, request.quantity);
                persistPlayerState(player->characterId, player->characterState);
                sendInventorySnapshot(*player);
                result.ok = true;
                result.message = "Purchased " + std::to_string(request.quantity) + " x " + item->name + ".";
            }
        }
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::MerchantBuyResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

void ZoneServer::handleMerchantSell(
    const std::shared_ptr<TcpConnection>& connection,
    const net::SerializedPacket& packet)
{
    net::MerchantSellRequestPayload request;
    net::MerchantSellResultPayload result;
    if (!net::deserializeClientPacket(packet, request))
    {
        result.message = "Invalid sell request.";
        sendClientPacket(
            connection,
            net::ClientPacketType::MerchantSellResult,
            packet.header.sequenceId,
            net::serialize(result),
            packet.header.sessionTokenHash);
        return;
    }

    PlayerEntity* player = findPlayerByConnection(connection);
    NpcEntity* npc = findNpcByEntityId(request.npcEntityId);
    const content::ItemDef* item = itemCatalog_.findItem(request.itemId);
    const content::NpcDef* npcDef = npc != nullptr ? npcCatalog_.findNpc(npc->npcId) : nullptr;

    if (player == nullptr || npc == nullptr || item == nullptr || npcDef == nullptr)
    {
        result.message = "Invalid merchant transaction.";
    }
    else if (!isNearNpc(*player, *npc))
    {
        result.message = "Too far from merchant.";
    }
    else if (request.quantity == 0)
    {
        result.message = "Invalid quantity.";
    }
    else
    {
        const bool acceptsItem = std::find(
            npcDef->merchantSellItemIds.begin(),
            npcDef->merchantSellItemIds.end(),
            request.itemId) != npcDef->merchantSellItemIds.end();
        if (!acceptsItem)
        {
            result.message = "Merchant does not buy that item.";
        }
        else if (player->characterState.itemQuantity(request.itemId) < request.quantity)
        {
            result.message = "Not enough items.";
        }
        else
        {
            const uint16_t merchantSkill = player->characterState.skillLevel("merchant");
            const uint32_t unitPrice = items::ItemRules::merchantSellPrice(
                *item,
                merchantSkill,
                player->characterState.cha);
            const uint32_t totalPrice = unitPrice * request.quantity;
            player->characterState.removeItem(request.itemId, request.quantity);
            player->characterState.gold += totalPrice;
            persistPlayerState(player->characterId, player->characterState);
            sendInventorySnapshot(*player);
            result.ok = true;
            result.message = "Sold " + std::to_string(request.quantity) + " x " + item->name + ".";
        }
    }

    sendClientPacket(
        connection,
        net::ClientPacketType::MerchantSellResult,
        packet.header.sequenceId,
        net::serialize(result),
        packet.header.sessionTokenHash);
}

} // namespace tbeq::server
