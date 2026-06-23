#include "ZoneServer.hpp"

#include <algorithm>

#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/skills/Progression.hpp"

namespace tbeq::server
{

net::SkillsSnapshotPayload ZoneServer::buildSkillsSnapshot(const CharacterState& state) const
{
    net::SkillsSnapshotPayload payload;
    payload.characterLevel = state.level;
    payload.characterExperience = state.experience;
    const uint32_t required = progression::experienceRequiredForLevel(static_cast<uint16_t>(state.level + 1));
    payload.experienceToNextLevel = required > state.experience ? required - state.experience : 0;

    for (const auto& [skillId, progress] : state.skills)
    {
        net::SkillEntryPayload entry;
        entry.skillId = skillId;
        entry.level = progress.level;
        entry.experience = progress.experience;
        entry.cap = skillResolver_.getCap(state.classId, skillId, state.level);
        if (entry.cap == 0)
        {
            continue;
        }
        payload.skills.push_back(std::move(entry));
    }

    std::sort(payload.skills.begin(), payload.skills.end(), [](const net::SkillEntryPayload& a, const net::SkillEntryPayload& b)
    {
        return a.skillId < b.skillId;
    });
    return payload;
}

void ZoneServer::sendSkillsSnapshot(const PlayerEntity& player)
{
    if (!player.connected || player.connection == nullptr)
    {
        return;
    }

    const auto payload = buildSkillsSnapshot(player.characterState);
    sendClientPacket(
        player.connection,
        net::ClientPacketType::SkillsSnapshot,
        0,
        net::serialize(payload),
        player.sessionTokenHash);
}

void ZoneServer::applyActivitySkillGain(PlayerEntity& player, const std::string& skillId, const std::string& activityId)
{
    if (combatManager_ == nullptr)
    {
        return;
    }

    const uint32_t amount = skillResolver_.activitySkillXpGain(activityId);
    combatManager_->grantSkillXp(player.characterId, skillId, amount);
    if (PlayerEntity* refreshed = findPlayerByCharacterId(player.characterId))
    {
        sendSkillsSnapshot(*refreshed);
    }
}

} // namespace tbeq::server
