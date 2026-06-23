#include "tbeq/net/PacketSerializer.hpp"

#include <cstring>
#include <stdexcept>

namespace tbeq::net
{

void appendRaw(std::vector<uint8_t>& buffer, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

void ByteWriter::writeU8(uint8_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU16(uint16_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU32(uint32_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeU64(uint64_t value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeF32(float value)
{
    appendRaw(buffer_, &value, sizeof(value));
}

void ByteWriter::writeBool(bool value)
{
    writeU8(value ? 1 : 0);
}

void ByteWriter::writeString(const std::string& value)
{
    writeU32(static_cast<uint32_t>(value.size()));
    appendRaw(buffer_, value.data(), value.size());
}

void ByteWriter::writeStringVector(const std::vector<std::string>& values)
{
    writeU32(static_cast<uint32_t>(values.size()));
    for (const auto& value : values)
    {
        writeString(value);
    }
}

ByteReader::ByteReader(std::span<const uint8_t> data)
    : data_(data)
{
}

bool ByteReader::hasRemaining() const
{
    return offset_ < data_.size();
}

bool ByteReader::readU8(uint8_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU16(uint16_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU32(uint32_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readU64(uint64_t& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readF32(float& out)
{
    if (offset_ + sizeof(out) > data_.size())
    {
        return false;
    }
    std::memcpy(&out, data_.data() + offset_, sizeof(out));
    offset_ += sizeof(out);
    return true;
}

bool ByteReader::readBool(bool& out)
{
    uint8_t value = 0;
    if (!readU8(value))
    {
        return false;
    }
    out = value != 0;
    return true;
}

bool ByteReader::readString(std::string& out)
{
    uint32_t length = 0;
    if (!readU32(length) || offset_ + length > data_.size())
    {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(data_.data() + offset_), length);
    offset_ += length;
    return true;
}

bool ByteReader::readStringVector(std::vector<std::string>& out)
{
    uint32_t count = 0;
    if (!readU32(count))
    {
        return false;
    }
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        std::string value;
        if (!readString(value))
        {
            return false;
        }
        out.push_back(std::move(value));
    }
    return true;
}

SerializedPacket serializeClientPacket(ClientPacketType type, uint32_t sequenceId, const ByteWriter& writer)
{
    SerializedPacket packet;
    packet.header.packetType = static_cast<uint16_t>(type);
    packet.header.sequenceId = sequenceId;
    packet.payload = writer.data();
    packet.header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    return packet;
}

SerializedPacket serializeServerPacket(ServerPacketType type, uint32_t sequenceId, const ByteWriter& writer)
{
    SerializedPacket packet;
    packet.header.packetType = static_cast<uint16_t>(type);
    packet.header.sequenceId = sequenceId;
    packet.payload = writer.data();
    packet.header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    return packet;
}

ByteWriter serialize(const ClientPingPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ClientPongPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const LoginRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.username);
    writer.writeString(payload.password);
    return writer;
}

ByteWriter serialize(const LoginResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU64(payload.sessionToken);
    writer.writeU64(payload.sessionTokenHash);
    return writer;
}

ByteWriter serialize(const CreateAccountRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.username);
    writer.writeString(payload.password);
    return writer;
}

ByteWriter serialize(const CreateAccountResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const CharacterListRequestPayload& payload)
{
    (void)payload;
    return ByteWriter{};
}

ByteWriter serialize(const CharacterSummaryPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeString(payload.name);
    writer.writeString(payload.raceId);
    writer.writeString(payload.classId);
    writer.writeU16(payload.level);
    writer.writeString(payload.zoneId);
    return writer;
}

ByteWriter serialize(const CharacterListResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU32(static_cast<uint32_t>(payload.characters.size()));
    for (const auto& character : payload.characters)
    {
        writer.writeString(character.characterId);
        writer.writeString(character.name);
        writer.writeString(character.raceId);
        writer.writeString(character.classId);
        writer.writeU16(character.level);
        writer.writeString(character.zoneId);
    }
    return writer;
}

ByteWriter serialize(const CreateCharacterRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.name);
    writer.writeString(payload.raceId);
    writer.writeString(payload.classId);
    return writer;
}

ByteWriter serialize(const CreateCharacterResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const SelectCharacterRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const ZoneConnectInfoPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeString(payload.zoneId);
    writer.writeString(payload.zoneHost);
    writer.writeU16(payload.zonePort);
    writer.writeU64(payload.sessionResumeToken);
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const SessionResumePayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeU64(payload.sessionResumeToken);
    return writer;
}

ByteWriter serialize(const SessionResumeResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU32(payload.entityId);
    writer.writeU32(static_cast<uint32_t>(payload.tileX));
    writer.writeU32(static_cast<uint32_t>(payload.tileY));
    return writer;
}

ByteWriter serialize(const ZoneSnapshotPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.zoneId);
    writer.writeString(payload.zoneName);
    writer.writeString(payload.tileStyle);
    writer.writeU32(static_cast<uint32_t>(payload.width));
    writer.writeU32(static_cast<uint32_t>(payload.height));
    writer.writeStringVector(payload.tiles);
    return writer;
}

ByteWriter serialize(const EntityStatePayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.entityId);
    writer.writeString(payload.name);
    writer.writeU8(payload.entityType);
    writer.writeU32(static_cast<uint32_t>(payload.tileX));
    writer.writeU32(static_cast<uint32_t>(payload.tileY));
    writer.writeString(payload.raceId);
    writer.writeString(payload.classId);
    writer.writeString(payload.appearanceId);
    writer.writeString(payload.equippedWeaponItemId);
    writer.writeString(payload.equippedHeadItemId);
    writer.writeString(payload.equippedChestItemId);
    writer.writeString(payload.equippedHandsItemId);
    return writer;
}

ByteWriter serialize(const EntitySnapshotPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(static_cast<uint32_t>(payload.entities.size()));
    for (const auto& entity : payload.entities)
    {
        const auto entityWriter = serialize(entity);
        writer.writeU32(static_cast<uint32_t>(entityWriter.data().size()));
        for (uint8_t byte : entityWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    return writer;
}

ByteWriter serialize(const MoveIntentPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(static_cast<uint32_t>(payload.targetTileX));
    writer.writeU32(static_cast<uint32_t>(payload.targetTileY));
    return writer;
}

ByteWriter serialize(const MoveResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU32(payload.entityId);
    writer.writeU32(static_cast<uint32_t>(payload.tileX));
    writer.writeU32(static_cast<uint32_t>(payload.tileY));
    return writer;
}

ByteWriter serialize(const ChatMessagePayload& payload)
{
    ByteWriter writer;
    writer.writeU8(payload.channel);
    writer.writeString(payload.text);
    return writer;
}

ByteWriter serialize(const ChatDeliverPayload& payload)
{
    ByteWriter writer;
    writer.writeU8(payload.channel);
    writer.writeString(payload.senderName);
    writer.writeString(payload.text);
    return writer;
}

ByteWriter serialize(const UsePortalPayload& payload)
{
    (void)payload;
    return ByteWriter{};
}

ByteWriter serialize(const UsePortalResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const RequestZoneTilesPayload& payload)
{
    (void)payload;
    return ByteWriter{};
}

ByteWriter serialize(const ZoneTileGridPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.zoneId);
    writer.writeStringVector(payload.tiles);
    return writer;
}

ByteWriter serialize(const CombatParticipantPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatSlot);
    writer.writeString(payload.name);
    writer.writeU8(payload.side);
    writer.writeU16(payload.hp);
    writer.writeU16(payload.maxHp);
    writer.writeU16(payload.mana);
    writer.writeU16(payload.maxMana);
    writer.writeBool(payload.isAlive);
    writer.writeBool(payload.isPlayerControlled);
    writer.writeBool(payload.isAiCompanion);
    writer.writeString(payload.classId);
    writer.writeString(payload.raceId);
    writer.writeString(payload.mobId);
    return writer;
}

ByteWriter serialize(const CombatStartPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatId);
    writer.writeU32(static_cast<uint32_t>(payload.participants.size()));
    for (const auto& participant : payload.participants)
    {
        const auto participantWriter = serialize(participant);
        writer.writeU32(static_cast<uint32_t>(participantWriter.data().size()));
        for (const uint8_t byte : participantWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    writer.writeU32(static_cast<uint32_t>(payload.turnOrder.size()));
    for (const uint32_t slot : payload.turnOrder)
    {
        writer.writeU32(slot);
    }
    writer.writeU32(payload.currentTurnIndex);
    writer.writeU32(payload.currentActorSlot);
    writer.writeU32(payload.turnDurationMs);
    return writer;
}

ByteWriter serialize(const CombatUpdatePayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatId);
    writer.writeU32(payload.currentTurnIndex);
    writer.writeU32(payload.currentActorSlot);
    writer.writeU32(payload.turnDurationMs);
    writer.writeU32(static_cast<uint32_t>(payload.participants.size()));
    for (const auto& participant : payload.participants)
    {
        const auto participantWriter = serialize(participant);
        writer.writeU32(static_cast<uint32_t>(participantWriter.data().size()));
        for (const uint8_t byte : participantWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    return writer;
}

ByteWriter serialize(const CombatEventPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatId);
    writer.writeU8(payload.eventType);
    writer.writeU32(payload.actorSlot);
    writer.writeU32(payload.targetSlot);
    writer.writeU32(static_cast<uint32_t>(payload.value));
    writer.writeString(payload.message);
    writer.writeString(payload.detail);
    return writer;
}

ByteWriter serialize(const CombatEndPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatId);
    writer.writeU8(payload.result);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const SubmitActionPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.combatId);
    writer.writeU8(payload.actionType);
    writer.writeU32(payload.targetCombatSlot);
    writer.writeString(payload.spellId);
    writer.writeString(payload.abilityId);
    return writer;
}

ByteWriter serialize(const SubmitActionResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const CharacterVitalsPayload& payload)
{
    ByteWriter writer;
    writer.writeU16(payload.hp);
    writer.writeU16(payload.maxHp);
    writer.writeU16(payload.mana);
    writer.writeU16(payload.maxMana);
    return writer;
}

ByteWriter serialize(const MeditateResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU16(payload.manaGained);
    writer.writeU16(payload.mana);
    writer.writeU16(payload.maxMana);
    return writer;
}

ByteWriter serialize(const SkillGainPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.skillId);
    writer.writeU16(payload.oldLevel);
    writer.writeU16(payload.newLevel);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const LevelUpPayload& payload)
{
    ByteWriter writer;
    writer.writeU16(payload.oldLevel);
    writer.writeU16(payload.newLevel);
    writer.writeU32(payload.experienceGranted);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const SkillEntryPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.skillId);
    writer.writeU16(payload.level);
    writer.writeU32(payload.experience);
    writer.writeU16(payload.cap);
    return writer;
}

ByteWriter serialize(const SkillsSnapshotPayload& payload)
{
    ByteWriter writer;
    writer.writeU16(payload.characterLevel);
    writer.writeU32(payload.characterExperience);
    writer.writeU32(payload.experienceToNextLevel);
    writer.writeU32(static_cast<uint32_t>(payload.skills.size()));
    for (const auto& skill : payload.skills)
    {
        const auto skillWriter = serialize(skill);
        writer.writeU32(static_cast<uint32_t>(skillWriter.data().size()));
        for (uint8_t byte : skillWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    return writer;
}

ByteWriter serialize(const InventoryEntryPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.itemId);
    writer.writeU16(payload.quantity);
    return writer;
}

ByteWriter serialize(const EquipmentEntryPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.slot);
    writer.writeString(payload.itemId);
    return writer;
}

ByteWriter serialize(const InventorySnapshotPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.gold);
    writer.writeU32(static_cast<uint32_t>(payload.items.size()));
    for (const auto& item : payload.items)
    {
        const auto itemWriter = serialize(item);
        writer.writeU32(static_cast<uint32_t>(itemWriter.data().size()));
        for (uint8_t byte : itemWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    writer.writeU32(static_cast<uint32_t>(payload.equipment.size()));
    for (const auto& entry : payload.equipment)
    {
        const auto entryWriter = serialize(entry);
        writer.writeU32(static_cast<uint32_t>(entryWriter.data().size()));
        for (uint8_t byte : entryWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    return writer;
}

ByteWriter serialize(const EquipItemRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.itemId);
    return writer;
}

ByteWriter serialize(const EquipItemResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const UnequipItemRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.slot);
    return writer;
}

ByteWriter serialize(const UnequipItemResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const NpcInteractRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.npcEntityId);
    return writer;
}

ByteWriter serialize(const MerchantStockEntryPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.itemId);
    writer.writeU16(payload.quantity);
    writer.writeU32(payload.buyPrice);
    writer.writeU32(payload.sellPrice);
    return writer;
}

ByteWriter serialize(const MerchantOpenPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.npcEntityId);
    writer.writeString(payload.npcName);
    writer.writeU32(static_cast<uint32_t>(payload.stock.size()));
    for (const auto& entry : payload.stock)
    {
        const auto entryWriter = serialize(entry);
        writer.writeU32(static_cast<uint32_t>(entryWriter.data().size()));
        for (uint8_t byte : entryWriter.data())
        {
            writer.writeU8(byte);
        }
    }
    writer.writeStringVector(payload.sellItemIds);
    return writer;
}

ByteWriter serialize(const MerchantBuyRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.npcEntityId);
    writer.writeString(payload.itemId);
    writer.writeU16(payload.quantity);
    return writer;
}

ByteWriter serialize(const MerchantBuyResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeBool(payload.stockUpdated);
    writer.writeString(payload.stockItemId);
    writer.writeU16(payload.stockQuantity);
    return writer;
}

ByteWriter serialize(const MerchantSellRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.npcEntityId);
    writer.writeString(payload.itemId);
    writer.writeU16(payload.quantity);
    return writer;
}

ByteWriter serialize(const MerchantSellResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const NpcDialogOpenPayload& payload)
{
    ByteWriter writer;
    writer.writeU32(payload.npcEntityId);
    writer.writeString(payload.npcName);
    writer.writeStringVector(payload.lines);
    return writer;
}

ByteWriter serialize(const SessionEndPayload& payload)
{
    (void)payload;
    return ByteWriter{};
}

ByteWriter serialize(const PlayerCommandRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.command);
    writer.writeStringVector(payload.args);
    return writer;
}

ByteWriter serialize(const PlayerCommandResultPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeU32(static_cast<uint32_t>(payload.tileX));
    writer.writeU32(static_cast<uint32_t>(payload.tileY));
    return writer;
}

ByteWriter serialize(const DebugCommandRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeU16(static_cast<uint16_t>(payload.command));
    writer.writeStringVector(payload.args);
    return writer;
}

ByteWriter serialize(const DebugCommandResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const ServerPingPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ServerPongPayload& payload)
{
    ByteWriter writer;
    writer.writeU64(payload.timestampMs);
    return writer;
}

ByteWriter serialize(const ZoneRegisterPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.zoneId);
    writer.writeString(payload.listenHost);
    writer.writeU16(payload.listenPort);
    writer.writeU32(payload.capacity);
    writer.writeString(payload.version);
    return writer;
}

ByteWriter serialize(const ZoneRegisterAckPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.accepted);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const PlayerEnterPreparePayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeU64(static_cast<uint64_t>(payload.accountId));
    writer.writeString(payload.zoneId);
    writer.writeU64(payload.sessionResumeToken);
    return writer;
}

ByteWriter serialize(const PlayerEnterReadyPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    return writer;
}

ByteWriter serialize(const LoadCharacterRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const LoadCharacterResponsePayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeString(payload.characterId);
    writer.writeString(payload.name);
    writer.writeString(payload.raceId);
    writer.writeString(payload.classId);
    writer.writeU16(payload.level);
    writer.writeString(payload.zoneId);
    writer.writeF32(payload.posX);
    writer.writeF32(payload.posY);
    writer.writeString(payload.stateJson);
    return writer;
}

ByteWriter serialize(const ZoneTransferRequestPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeString(payload.sourceZoneId);
    writer.writeString(payload.destZoneId);
    writer.writeU32(static_cast<uint32_t>(payload.destTileX));
    writer.writeU32(static_cast<uint32_t>(payload.destTileY));
    writer.writeU64(payload.sessionResumeToken);
    return writer;
}

ByteWriter serialize(const ZoneTransferAuthorizePayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const ZoneTransferCompletePayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    writer.writeString(payload.zoneId);
    writer.writeF32(payload.posX);
    writer.writeF32(payload.posY);
    return writer;
}

ByteWriter serialize(const PlayerDisconnectPayload& payload)
{
    ByteWriter writer;
    writer.writeString(payload.characterId);
    return writer;
}

ByteWriter serialize(const ZoneConnectForwardPayload& payload)
{
    ByteWriter writer;
    writer.writeBool(payload.ok);
    writer.writeString(payload.message);
    writer.writeString(payload.zoneId);
    writer.writeString(payload.zoneHost);
    writer.writeU16(payload.zonePort);
    writer.writeU64(payload.sessionResumeToken);
    writer.writeString(payload.characterId);
    return writer;
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientPingPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::Ping))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientPongPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::Pong))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeClientPacket(const SerializedPacket& packet, LoginRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::LoginRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.username) && reader.readString(out.password);
}

bool deserializeClientPacket(const SerializedPacket& packet, LoginResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::LoginResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readU64(out.sessionToken)
        && reader.readU64(out.sessionTokenHash);
}

bool deserializeClientPacket(const SerializedPacket& packet, CreateAccountRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CreateAccountRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.username) && reader.readString(out.password);
}

bool deserializeClientPacket(const SerializedPacket& packet, CreateAccountResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CreateAccountResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, CharacterListRequestPayload& out)
{
    (void)out;
    return packet.header.packetType == static_cast<uint16_t>(ClientPacketType::CharacterListRequest);
}

bool deserializeClientPacket(const SerializedPacket& packet, CharacterListResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CharacterListResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t count = 0;
    if (!reader.readBool(out.ok) || !reader.readString(out.message) || !reader.readU32(count))
    {
        return false;
    }

    out.characters.clear();
    out.characters.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        CharacterSummaryPayload character;
        if (!reader.readString(character.characterId)
            || !reader.readString(character.name)
            || !reader.readString(character.raceId)
            || !reader.readString(character.classId)
            || !reader.readU16(character.level)
            || !reader.readString(character.zoneId))
        {
            return false;
        }
        out.characters.push_back(std::move(character));
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, CreateCharacterRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CreateCharacterRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.name)
        && reader.readString(out.raceId)
        && reader.readString(out.classId);
}

bool deserializeClientPacket(const SerializedPacket& packet, CreateCharacterResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CreateCharacterResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readString(out.characterId);
}

bool deserializeClientPacket(const SerializedPacket& packet, SelectCharacterRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SelectCharacterRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId);
}

bool deserializeClientPacket(const SerializedPacket& packet, ZoneConnectInfoPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::ZoneConnectInfo))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readString(out.zoneId)
        && reader.readString(out.zoneHost)
        && reader.readU16(out.zonePort)
        && reader.readU64(out.sessionResumeToken)
        && reader.readString(out.characterId);
}

bool deserializeClientPacket(const SerializedPacket& packet, SessionResumePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SessionResume))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId) && reader.readU64(out.sessionResumeToken);
}

bool deserializeClientPacket(const SerializedPacket& packet, SessionResumeResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SessionResumeResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    if (!reader.readBool(out.ok) || !reader.readString(out.message))
    {
        return false;
    }
    if (!reader.readU32(out.entityId))
    {
        out.entityId = 0;
        out.tileX = 0;
        out.tileY = 0;
        return true;
    }
    if (!reader.readU32(tileX) || !reader.readU32(tileY))
    {
        return false;
    }
    out.tileX = static_cast<int32_t>(tileX);
    out.tileY = static_cast<int32_t>(tileY);
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, ZoneSnapshotPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::ZoneSnapshot))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t width = 0;
    uint32_t height = 0;
    if (!reader.readString(out.zoneId)
        || !reader.readString(out.zoneName)
        || !reader.readString(out.tileStyle)
        || !reader.readU32(width)
        || !reader.readU32(height)
        || !reader.readStringVector(out.tiles))
    {
        return false;
    }
    out.width = static_cast<int32_t>(width);
    out.height = static_cast<int32_t>(height);
    return true;
}

bool deserializeEntityState(ByteReader& reader, EntityStatePayload& out)
{
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    if (!reader.readU32(out.entityId)
        || !reader.readString(out.name)
        || !reader.readU8(out.entityType)
        || !reader.readU32(tileX)
        || !reader.readU32(tileY))
    {
        return false;
    }
    out.tileX = static_cast<int32_t>(tileX);
    out.tileY = static_cast<int32_t>(tileY);

    out.raceId.clear();
    out.classId.clear();
    out.appearanceId.clear();
    if (reader.hasRemaining())
    {
        if (!reader.readString(out.raceId)
            || !reader.readString(out.classId)
            || !reader.readString(out.appearanceId))
        {
            return false;
        }
    }
    out.equippedWeaponItemId.clear();
    if (reader.hasRemaining())
    {
        if (!reader.readString(out.equippedWeaponItemId))
        {
            return false;
        }
    }
    out.equippedHeadItemId.clear();
    out.equippedChestItemId.clear();
    out.equippedHandsItemId.clear();
    if (reader.hasRemaining())
    {
        if (!reader.readString(out.equippedHeadItemId)
            || !reader.readString(out.equippedChestItemId)
            || !reader.readString(out.equippedHandsItemId))
        {
            return false;
        }
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, EntitySnapshotPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::EntitySnapshot))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t count = 0;
    if (!reader.readU32(count))
    {
        return false;
    }

    out.entities.clear();
    out.entities.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t entitySize = 0;
        if (!reader.readU32(entitySize))
        {
            return false;
        }
        const std::size_t offset = reader.hasRemaining() ? 0 : 0;
        (void)offset;
        std::vector<uint8_t> entityBytes(entitySize);
        for (uint32_t b = 0; b < entitySize; ++b)
        {
            uint8_t byte = 0;
            if (!reader.readU8(byte))
            {
                return false;
            }
            entityBytes[b] = byte;
        }
        ByteReader entityReader(entityBytes);
        EntityStatePayload entity;
        if (!deserializeEntityState(entityReader, entity))
        {
            return false;
        }
        out.entities.push_back(std::move(entity));
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, MoveIntentPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MoveIntent))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    if (!reader.readU32(tileX) || !reader.readU32(tileY))
    {
        return false;
    }
    out.targetTileX = static_cast<int32_t>(tileX);
    out.targetTileY = static_cast<int32_t>(tileY);
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, MoveResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MoveResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    if (!reader.readBool(out.ok)
        || !reader.readString(out.message)
        || !reader.readU32(out.entityId)
        || !reader.readU32(tileX)
        || !reader.readU32(tileY))
    {
        return false;
    }
    out.tileX = static_cast<int32_t>(tileX);
    out.tileY = static_cast<int32_t>(tileY);
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, ChatMessagePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::ChatMessage))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU8(out.channel) && reader.readString(out.text);
}

bool deserializeClientPacket(const SerializedPacket& packet, ChatDeliverPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::ChatDeliver))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU8(out.channel)
        && reader.readString(out.senderName)
        && reader.readString(out.text);
}

bool deserializeClientPacket(const SerializedPacket& packet, UsePortalPayload& out)
{
    (void)out;
    return packet.header.packetType == static_cast<uint16_t>(ClientPacketType::UsePortal);
}

bool deserializeClientPacket(const SerializedPacket& packet, UsePortalResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::UsePortalResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, RequestZoneTilesPayload& out)
{
    (void)out;
    return packet.header.packetType == static_cast<uint16_t>(ClientPacketType::RequestZoneTiles);
}

namespace
{

bool readCombatParticipant(ByteReader& reader, CombatParticipantPayload& out)
{
    return reader.readU32(out.combatSlot)
        && reader.readString(out.name)
        && reader.readU8(out.side)
        && reader.readU16(out.hp)
        && reader.readU16(out.maxHp)
        && reader.readU16(out.mana)
        && reader.readU16(out.maxMana)
        && reader.readBool(out.isAlive)
        && reader.readBool(out.isPlayerControlled)
        && reader.readBool(out.isAiCompanion)
        && reader.readString(out.classId)
        && reader.readString(out.raceId)
        && reader.readString(out.mobId);
}

bool readEmbeddedCombatParticipant(ByteReader& reader, CombatParticipantPayload& out)
{
    uint32_t byteCount = 0;
    if (!reader.readU32(byteCount))
    {
        return false;
    }

    std::vector<uint8_t> bytes(byteCount);
    for (uint32_t i = 0; i < byteCount; ++i)
    {
        uint8_t value = 0;
        if (!reader.readU8(value))
        {
            return false;
        }
        bytes[i] = value;
    }

    ByteReader nested(bytes);
    return readCombatParticipant(nested, out);
}

} // namespace

bool deserializeClientPacket(const SerializedPacket& packet, CombatStartPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CombatStart))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t participantCount = 0;
    if (!reader.readU32(out.combatId) || !reader.readU32(participantCount))
    {
        return false;
    }

    out.participants.clear();
    out.participants.reserve(participantCount);
    for (uint32_t i = 0; i < participantCount; ++i)
    {
        CombatParticipantPayload participant;
        if (!readEmbeddedCombatParticipant(reader, participant))
        {
            return false;
        }
        out.participants.push_back(std::move(participant));
    }

    uint32_t turnCount = 0;
    if (!reader.readU32(turnCount))
    {
        return false;
    }
    out.turnOrder.clear();
    out.turnOrder.reserve(turnCount);
    for (uint32_t i = 0; i < turnCount; ++i)
    {
        uint32_t slot = 0;
        if (!reader.readU32(slot))
        {
            return false;
        }
        out.turnOrder.push_back(slot);
    }

    return reader.readU32(out.currentTurnIndex)
        && reader.readU32(out.currentActorSlot)
        && reader.readU32(out.turnDurationMs);
}

bool deserializeClientPacket(const SerializedPacket& packet, CombatUpdatePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CombatUpdate))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t participantCount = 0;
    if (!reader.readU32(out.combatId)
        || !reader.readU32(out.currentTurnIndex)
        || !reader.readU32(out.currentActorSlot)
        || !reader.readU32(out.turnDurationMs)
        || !reader.readU32(participantCount))
    {
        return false;
    }

    out.participants.clear();
    out.participants.reserve(participantCount);
    for (uint32_t i = 0; i < participantCount; ++i)
    {
        CombatParticipantPayload participant;
        if (!readEmbeddedCombatParticipant(reader, participant))
        {
            return false;
        }
        out.participants.push_back(std::move(participant));
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, CombatEventPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CombatEvent))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t value = 0;
    return reader.readU32(out.combatId)
        && reader.readU8(out.eventType)
        && reader.readU32(out.actorSlot)
        && reader.readU32(out.targetSlot)
        && reader.readU32(value)
        && reader.readString(out.message)
        && reader.readString(out.detail)
        && (out.value = static_cast<int32_t>(value), true);
}

bool deserializeClientPacket(const SerializedPacket& packet, CombatEndPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CombatEnd))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readU32(out.combatId)
        && reader.readU8(out.result)
        && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, SubmitActionPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SubmitAction))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readU32(out.combatId)
        && reader.readU8(out.actionType)
        && reader.readU32(out.targetCombatSlot)
        && reader.readString(out.spellId)
        && reader.readString(out.abilityId);
}

bool deserializeClientPacket(const SerializedPacket& packet, SubmitActionResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SubmitActionResult))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, CharacterVitalsPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::CharacterVitals))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readU16(out.hp)
        && reader.readU16(out.maxHp)
        && reader.readU16(out.mana)
        && reader.readU16(out.maxMana);
}

bool deserializeClientPacket(const SerializedPacket& packet, MeditateResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MeditateResult))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readU16(out.manaGained)
        && reader.readU16(out.mana)
        && reader.readU16(out.maxMana);
}

bool deserializeClientPacket(const SerializedPacket& packet, SkillGainPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SkillGain))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readString(out.skillId)
        && reader.readU16(out.oldLevel)
        && reader.readU16(out.newLevel)
        && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, LevelUpPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::LevelUp))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    return reader.readU16(out.oldLevel)
        && reader.readU16(out.newLevel)
        && reader.readU32(out.experienceGranted)
        && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, SkillsSnapshotPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::SkillsSnapshot))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t skillCount = 0;
    if (!reader.readU16(out.characterLevel)
        || !reader.readU32(out.characterExperience)
        || !reader.readU32(out.experienceToNextLevel)
        || !reader.readU32(skillCount))
    {
        return false;
    }

    out.skills.clear();
    out.skills.reserve(skillCount);
    for (uint32_t i = 0; i < skillCount; ++i)
    {
        uint32_t entrySize = 0;
        if (!reader.readU32(entrySize))
        {
            return false;
        }
        std::vector<uint8_t> entryBytes(entrySize);
        for (uint32_t b = 0; b < entrySize; ++b)
        {
            if (!reader.readU8(entryBytes[b]))
            {
                return false;
            }
        }
        ByteReader entryReader(entryBytes);
        SkillEntryPayload entry;
        if (!entryReader.readString(entry.skillId)
            || !entryReader.readU16(entry.level)
            || !entryReader.readU32(entry.experience)
            || !entryReader.readU16(entry.cap))
        {
            return false;
        }
        out.skills.push_back(std::move(entry));
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, InventorySnapshotPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::InventorySnapshot))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t itemCount = 0;
    if (!reader.readU32(out.gold) || !reader.readU32(itemCount))
    {
        return false;
    }

    out.items.clear();
    out.items.reserve(itemCount);
    for (uint32_t i = 0; i < itemCount; ++i)
    {
        uint32_t entrySize = 0;
        if (!reader.readU32(entrySize))
        {
            return false;
        }
        std::vector<uint8_t> entryBytes(entrySize);
        for (uint32_t b = 0; b < entrySize; ++b)
        {
            uint8_t byte = 0;
            if (!reader.readU8(byte))
            {
                return false;
            }
            entryBytes[b] = byte;
        }
        ByteReader entryReader(entryBytes);
        InventoryEntryPayload entry;
        if (!entryReader.readString(entry.itemId) || !entryReader.readU16(entry.quantity))
        {
            return false;
        }
        out.items.push_back(std::move(entry));
    }

    uint32_t equipmentCount = 0;
    if (!reader.readU32(equipmentCount))
    {
        return false;
    }
    out.equipment.clear();
    out.equipment.reserve(equipmentCount);
    for (uint32_t i = 0; i < equipmentCount; ++i)
    {
        uint32_t entrySize = 0;
        if (!reader.readU32(entrySize))
        {
            return false;
        }
        std::vector<uint8_t> entryBytes(entrySize);
        for (uint32_t b = 0; b < entrySize; ++b)
        {
            uint8_t byte = 0;
            if (!reader.readU8(byte))
            {
                return false;
            }
            entryBytes[b] = byte;
        }
        ByteReader entryReader(entryBytes);
        EquipmentEntryPayload entry;
        if (!entryReader.readString(entry.slot) || !entryReader.readString(entry.itemId))
        {
            return false;
        }
        out.equipment.push_back(std::move(entry));
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, EquipItemRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::EquipItemRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.itemId);
}

bool deserializeClientPacket(const SerializedPacket& packet, EquipItemResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::EquipItemResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, UnequipItemRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::UnequipItemRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.slot);
}

bool deserializeClientPacket(const SerializedPacket& packet, UnequipItemResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::UnequipItemResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, NpcInteractRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::NpcInteractRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU32(out.npcEntityId);
}

bool deserializeClientPacket(const SerializedPacket& packet, MerchantOpenPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MerchantOpen))
    {
        return false;
    }

    ByteReader reader(packet.payload);
    uint32_t stockCount = 0;
    if (!reader.readU32(out.npcEntityId)
        || !reader.readString(out.npcName)
        || !reader.readU32(stockCount))
    {
        return false;
    }

    out.stock.clear();
    out.stock.reserve(stockCount);
    for (uint32_t i = 0; i < stockCount; ++i)
    {
        uint32_t entrySize = 0;
        if (!reader.readU32(entrySize))
        {
            return false;
        }
        std::vector<uint8_t> entryBytes(entrySize);
        for (uint32_t b = 0; b < entrySize; ++b)
        {
            uint8_t byte = 0;
            if (!reader.readU8(byte))
            {
                return false;
            }
            entryBytes[b] = byte;
        }
        ByteReader entryReader(entryBytes);
        MerchantStockEntryPayload entry;
        if (!entryReader.readString(entry.itemId)
            || !entryReader.readU16(entry.quantity)
            || !entryReader.readU32(entry.buyPrice)
            || !entryReader.readU32(entry.sellPrice))
        {
            return false;
        }
        out.stock.push_back(std::move(entry));
    }

    return reader.readStringVector(out.sellItemIds);
}

bool deserializeClientPacket(const SerializedPacket& packet, MerchantBuyRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MerchantBuyRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU32(out.npcEntityId)
        && reader.readString(out.itemId)
        && reader.readU16(out.quantity);
}

bool deserializeClientPacket(const SerializedPacket& packet, MerchantBuyResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MerchantBuyResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    out.stockUpdated = false;
    out.stockItemId.clear();
    out.stockQuantity = 0;
    if (!reader.readBool(out.ok) || !reader.readString(out.message))
    {
        return false;
    }
    if (reader.hasRemaining())
    {
        if (!reader.readBool(out.stockUpdated)
            || !reader.readString(out.stockItemId)
            || !reader.readU16(out.stockQuantity))
        {
            return false;
        }
    }
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, MerchantSellRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MerchantSellRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU32(out.npcEntityId)
        && reader.readString(out.itemId)
        && reader.readU16(out.quantity);
}

bool deserializeClientPacket(const SerializedPacket& packet, MerchantSellResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::MerchantSellResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeClientPacket(const SerializedPacket& packet, NpcDialogOpenPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::NpcDialogOpen))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU32(out.npcEntityId)
        && reader.readString(out.npcName)
        && reader.readStringVector(out.lines);
}

bool deserializeClientPacket(const SerializedPacket& packet, SessionEndPayload& out)
{
    (void)out;
    return packet.header.packetType == static_cast<uint16_t>(ClientPacketType::SessionEnd);
}

bool deserializeClientPacket(const SerializedPacket& packet, PlayerCommandRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::PlayerCommandRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.command) && reader.readStringVector(out.args);
}

bool deserializeClientPacket(const SerializedPacket& packet, PlayerCommandResultPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::PlayerCommandResult))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    if (!reader.readBool(out.ok)
        || !reader.readString(out.message)
        || !reader.readU32(tileX)
        || !reader.readU32(tileY))
    {
        return false;
    }
    out.tileX = static_cast<int32_t>(tileX);
    out.tileY = static_cast<int32_t>(tileY);
    return true;
}

bool deserializeClientPacket(const SerializedPacket& packet, ZoneTileGridPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::ZoneTileGrid))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.zoneId) && reader.readStringVector(out.tiles);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::DebugCommandRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint16_t command = 0;
    if (!reader.readU16(command))
    {
        return false;
    }
    out.command = static_cast<DebugCommand>(command);
    return reader.readStringVector(out.args);
}

bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ClientPacketType::DebugCommandResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerPingPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::Ping))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerPongPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::Pong))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readU64(out.timestampMs);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneRegister))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.zoneId)
        && reader.readString(out.listenHost)
        && reader.readU16(out.listenPort)
        && reader.readU32(out.capacity)
        && reader.readString(out.version);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterAckPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneRegisterAck))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.accepted) && reader.readString(out.message);
}

bool deserializeServerPacket(const SerializedPacket& packet, PlayerEnterPreparePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::PlayerEnterPrepare))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint64_t accountId = 0;
    if (!reader.readString(out.characterId)
        || !reader.readU64(accountId)
        || !reader.readString(out.zoneId)
        || !reader.readU64(out.sessionResumeToken))
    {
        return false;
    }
    out.accountId = static_cast<int64_t>(accountId);
    return true;
}

bool deserializeServerPacket(const SerializedPacket& packet, PlayerEnterReadyPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::PlayerEnterReady))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId)
        && reader.readBool(out.ok)
        && reader.readString(out.message);
}

bool deserializeServerPacket(const SerializedPacket& packet, LoadCharacterRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::LoadCharacterRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId);
}

bool deserializeServerPacket(const SerializedPacket& packet, LoadCharacterResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::LoadCharacterResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readString(out.characterId)
        && reader.readString(out.name)
        && reader.readString(out.raceId)
        && reader.readString(out.classId)
        && reader.readU16(out.level)
        && reader.readString(out.zoneId)
        && reader.readF32(out.posX)
        && reader.readF32(out.posY)
        && reader.readString(out.stateJson);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneTransferRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint32_t destX = 0;
    uint32_t destY = 0;
    if (!reader.readString(out.characterId)
        || !reader.readString(out.sourceZoneId)
        || !reader.readString(out.destZoneId)
        || !reader.readU32(destX)
        || !reader.readU32(destY)
        || !reader.readU64(out.sessionResumeToken))
    {
        return false;
    }
    out.destTileX = static_cast<int32_t>(destX);
    out.destTileY = static_cast<int32_t>(destY);
    return true;
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferAuthorizePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneTransferAuthorize))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferCompletePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneTransferComplete))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId)
        && reader.readString(out.zoneId)
        && reader.readF32(out.posX)
        && reader.readF32(out.posY);
}

bool deserializeServerPacket(const SerializedPacket& packet, PlayerDisconnectPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::PlayerDisconnect))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readString(out.characterId);
}

bool deserializeServerPacket(const SerializedPacket& packet, ZoneConnectForwardPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::ZoneConnectForward))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok)
        && reader.readString(out.message)
        && reader.readString(out.zoneId)
        && reader.readString(out.zoneHost)
        && reader.readU16(out.zonePort)
        && reader.readU64(out.sessionResumeToken)
        && reader.readString(out.characterId);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandRequestPayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::DebugCommandRequest))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    uint16_t command = 0;
    if (!reader.readU16(command))
    {
        return false;
    }
    out.command = static_cast<DebugCommand>(command);
    return reader.readStringVector(out.args);
}

bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandResponsePayload& out)
{
    if (packet.header.packetType != static_cast<uint16_t>(ServerPacketType::DebugCommandResponse))
    {
        return false;
    }
    ByteReader reader(packet.payload);
    return reader.readBool(out.ok) && reader.readString(out.message);
}

bool readPacketHeader(std::span<const uint8_t> data, PacketHeader& header)
{
    if (data.size() < sizeof(PacketHeader))
    {
        return false;
    }
    std::memcpy(&header, data.data(), sizeof(PacketHeader));
    return header.isValid();
}

std::vector<uint8_t> encodePacket(const SerializedPacket& packet)
{
    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(PacketHeader) + packet.payload.size());
    PacketHeader header = packet.header;
    header.payloadLength = static_cast<uint32_t>(packet.payload.size());
    std::memcpy(buffer.data(), &header, sizeof(PacketHeader));
    if (!packet.payload.empty())
    {
        std::memcpy(buffer.data() + sizeof(PacketHeader), packet.payload.data(), packet.payload.size());
    }
    return buffer;
}

bool decodePacket(std::span<const uint8_t> data, SerializedPacket& out)
{
    if (!readPacketHeader(data, out.header))
    {
        return false;
    }
    if (data.size() < sizeof(PacketHeader) + out.header.payloadLength)
    {
        return false;
    }
    out.payload.assign(
        data.begin() + sizeof(PacketHeader),
        data.begin() + sizeof(PacketHeader) + out.header.payloadLength);
    return true;
}

} // namespace tbeq::net
