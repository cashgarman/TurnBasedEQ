#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketHeader.hpp"
#include "tbeq/net/ServerPackets.hpp"

namespace tbeq::net
{

class ByteWriter
{
public:
    void writeU8(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeU64(uint64_t value);
    void writeF32(float value);
    void writeBool(bool value);
    void writeString(const std::string& value);
    void writeStringVector(const std::vector<std::string>& values);

    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t>&& take() { return std::move(buffer_); }

private:
    std::vector<uint8_t> buffer_;
};

class ByteReader
{
public:
    explicit ByteReader(std::span<const uint8_t> data);

    bool hasRemaining() const;
    bool readU8(uint8_t& out);
    bool readU16(uint16_t& out);
    bool readU32(uint32_t& out);
    bool readU64(uint64_t& out);
    bool readF32(float& out);
    bool readBool(bool& out);
    bool readString(std::string& out);
    bool readStringVector(std::vector<std::string>& out);

private:
    std::span<const uint8_t> data_;
    std::size_t offset_ = 0;
};

struct SerializedPacket
{
    PacketHeader header;
    std::vector<uint8_t> payload;
};

SerializedPacket serializeClientPacket(ClientPacketType type, uint32_t sequenceId, const ByteWriter& writer);
SerializedPacket serializeServerPacket(ServerPacketType type, uint32_t sequenceId, const ByteWriter& writer);

bool deserializeClientPacket(const SerializedPacket& packet, ClientPingPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientPongPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, LoginRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, LoginResponsePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CreateAccountRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CreateAccountResponsePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CharacterListRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CharacterListResponsePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CreateCharacterRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CreateCharacterResponsePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SelectCharacterRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ZoneConnectInfoPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SessionResumePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SessionResumeResponsePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ZoneSnapshotPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, EntitySnapshotPayload& out);
bool deserializeEntityState(ByteReader& reader, EntityStatePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, MoveIntentPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, MoveResultPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ChatMessagePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ChatDeliverPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, UsePortalPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, UsePortalResultPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, RequestZoneTilesPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ZoneTileGridPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CombatStartPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CombatUpdatePayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CombatEventPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CombatEndPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SubmitActionPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SubmitActionResultPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, CharacterVitalsPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, MeditateResultPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, SkillGainPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandRequestPayload& out);
bool deserializeClientPacket(const SerializedPacket& packet, ClientDebugCommandResponsePayload& out);

bool deserializeServerPacket(const SerializedPacket& packet, ServerPingPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerPongPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneRegisterAckPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, PlayerEnterPreparePayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, PlayerEnterReadyPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, LoadCharacterRequestPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, LoadCharacterResponsePayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferRequestPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferAuthorizePayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneTransferCompletePayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, PlayerDisconnectPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ZoneConnectForwardPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandRequestPayload& out);
bool deserializeServerPacket(const SerializedPacket& packet, ServerDebugCommandResponsePayload& out);

ByteWriter serialize(const ClientPingPayload& payload);
ByteWriter serialize(const ClientPongPayload& payload);
ByteWriter serialize(const LoginRequestPayload& payload);
ByteWriter serialize(const LoginResponsePayload& payload);
ByteWriter serialize(const CreateAccountRequestPayload& payload);
ByteWriter serialize(const CreateAccountResponsePayload& payload);
ByteWriter serialize(const CharacterListRequestPayload& payload);
ByteWriter serialize(const CharacterSummaryPayload& payload);
ByteWriter serialize(const CharacterListResponsePayload& payload);
ByteWriter serialize(const CreateCharacterRequestPayload& payload);
ByteWriter serialize(const CreateCharacterResponsePayload& payload);
ByteWriter serialize(const SelectCharacterRequestPayload& payload);
ByteWriter serialize(const ZoneConnectInfoPayload& payload);
ByteWriter serialize(const SessionResumePayload& payload);
ByteWriter serialize(const SessionResumeResponsePayload& payload);
ByteWriter serialize(const ZoneSnapshotPayload& payload);
ByteWriter serialize(const EntityStatePayload& payload);
ByteWriter serialize(const EntitySnapshotPayload& payload);
ByteWriter serialize(const MoveIntentPayload& payload);
ByteWriter serialize(const MoveResultPayload& payload);
ByteWriter serialize(const ChatMessagePayload& payload);
ByteWriter serialize(const ChatDeliverPayload& payload);
ByteWriter serialize(const UsePortalPayload& payload);
ByteWriter serialize(const UsePortalResultPayload& payload);
ByteWriter serialize(const RequestZoneTilesPayload& payload);
ByteWriter serialize(const ZoneTileGridPayload& payload);
ByteWriter serialize(const CombatParticipantPayload& payload);
ByteWriter serialize(const CombatStartPayload& payload);
ByteWriter serialize(const CombatUpdatePayload& payload);
ByteWriter serialize(const CombatEventPayload& payload);
ByteWriter serialize(const CombatEndPayload& payload);
ByteWriter serialize(const SubmitActionPayload& payload);
ByteWriter serialize(const SubmitActionResultPayload& payload);
ByteWriter serialize(const CharacterVitalsPayload& payload);
ByteWriter serialize(const MeditateResultPayload& payload);
ByteWriter serialize(const SkillGainPayload& payload);
ByteWriter serialize(const DebugCommandRequestPayload& payload);
ByteWriter serialize(const DebugCommandResponsePayload& payload);

ByteWriter serialize(const ServerPingPayload& payload);
ByteWriter serialize(const ServerPongPayload& payload);
ByteWriter serialize(const ZoneRegisterPayload& payload);
ByteWriter serialize(const ZoneRegisterAckPayload& payload);
ByteWriter serialize(const PlayerEnterPreparePayload& payload);
ByteWriter serialize(const PlayerEnterReadyPayload& payload);
ByteWriter serialize(const LoadCharacterRequestPayload& payload);
ByteWriter serialize(const LoadCharacterResponsePayload& payload);
ByteWriter serialize(const ZoneTransferRequestPayload& payload);
ByteWriter serialize(const ZoneTransferAuthorizePayload& payload);
ByteWriter serialize(const ZoneTransferCompletePayload& payload);
ByteWriter serialize(const PlayerDisconnectPayload& payload);
ByteWriter serialize(const ZoneConnectForwardPayload& payload);

bool readPacketHeader(std::span<const uint8_t> data, PacketHeader& header);
std::vector<uint8_t> encodePacket(const SerializedPacket& packet);
bool decodePacket(std::span<const uint8_t> data, SerializedPacket& out);

} // namespace tbeq::net
