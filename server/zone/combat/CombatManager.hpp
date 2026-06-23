#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <asio.hpp>

#include "tbeq/ai/ClassCombatBrain.hpp"
#include "tbeq/ai/ClassCombatProfile.hpp"
#include "tbeq/ai/PartyMemberAI.hpp"
#include "tbeq/combat/CombatInstance.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/skills/SkillResolver.hpp"

namespace tbeq::server
{

class CombatManager
{
public:
    struct PlayerView
    {
        std::string characterId;
        std::string name;
        std::string classId;
        uint16_t level = 1;
        CharacterState* state = nullptr;
        bool* inCombat = nullptr;
        uint32_t* combatId = nullptr;
        uint32_t* combatSlot = nullptr;
        bool isAiCompanion = false;
    };

    using FindPlayerFn = std::function<PlayerView*(const std::string& characterId)>;
    using FindAiCompanionsFn = std::function<std::vector<PlayerView>(const std::string& leaderCharacterId)>;
    using BroadcastFn = std::function<void(const std::vector<std::string>& characterIds, net::ClientPacketType type, const net::ByteWriter& writer)>;
    using PersistStateFn = std::function<void(const std::string& characterId, const CharacterState& state)>;

    CombatManager(
        asio::io_context& io,
        SkillResolver& skillResolver,
        const content::MobCatalog& mobCatalog,
        const content::SpellCatalog& spellCatalog,
        const content::AbilityCatalog& abilityCatalog,
        const ai::ClassCombatProfileCatalog& aiProfiles,
        FindPlayerFn findPlayer,
        FindAiCompanionsFn findAiCompanions,
        BroadcastFn broadcast,
        PersistStateFn persistState);

    bool tryStartSpawnCombat(PlayerView& player, const std::string& mobTable);
    bool startDebugCombat(PlayerView& player, const std::vector<std::string>& mobIds);
    bool submitAction(
        const std::string& characterId,
        const net::SubmitActionPayload& request,
        net::SubmitActionResultPayload& result);
    bool forceEndCombat(const std::string& characterId, combat::CombatOutcome outcome);
    bool killTargetInCombat(const std::string& characterId, uint32_t targetSlot);
    void setGodMode(const std::string& characterId, bool enabled);
    void fillMana(const std::string& characterId);
    void unlockAllSpells(const std::string& characterId);

    bool isInCombat(const std::string& characterId) const;

private:
    struct ActiveCombat
    {
        std::unique_ptr<combat::CombatInstance> instance;
        std::vector<std::string> playerCharacterIds;
        std::unordered_map<uint32_t, std::string> slotToCharacterId;
        std::shared_ptr<asio::steady_timer> turnTimer;
    };

    uint32_t allocateCombatId();
    net::CombatParticipantPayload toPayload(const combat::CombatParticipant& participant) const;
    net::CombatStartPayload buildStartPayload(const combat::CombatInstance& instance) const;
    net::CombatUpdatePayload buildUpdatePayload(const combat::CombatInstance& instance) const;
    net::CombatEventPayload toEventPayload(uint32_t combatId, const combat::CombatEvent& event) const;

    void syncParticipantFromPlayer(combat::CombatParticipant& participant, const PlayerView& player);
    void syncPlayerFromParticipant(const combat::CombatParticipant& participant, PlayerView& player);
    void applySkillXp(PlayerView& player, bool hit);
    void grantLoot(PlayerView& player, const std::vector<combat::CombatLootRoll>& loot);
    void addPlayerToCombat(ActiveCombat& combat, PlayerView& player, bool playerControlled, bool aiCompanion);
    void addAiCompanionsToCombat(ActiveCombat& combat, const std::string& leaderCharacterId);

    ActiveCombat* findCombatForCharacter(const std::string& characterId);
    const ActiveCombat* findCombatForCharacter(const std::string& characterId) const;
    void broadcastCombatStart(ActiveCombat& combat);
    void broadcastUpdate(ActiveCombat& combat);
    void broadcastEvents(ActiveCombat& combat, const std::vector<combat::CombatEvent>& events);
    void syncAllParticipants(ActiveCombat& combat);
    void beginTurn(ActiveCombat& combat);
    void resolveEnemyTurn(ActiveCombat& combat);
    void resolveAiCompanionTurn(ActiveCombat& combat);
    void onTurnTimeout(uint32_t combatId, uint32_t actorSlot);
    void finishCombat(ActiveCombat& combat);

    asio::io_context& io_;
    SkillResolver& skillResolver_;
    const content::MobCatalog& mobCatalog_;
    const content::SpellCatalog& spellCatalog_;
    const content::AbilityCatalog& abilityCatalog_;
    ai::ClassCombatBrain classCombatBrain_;
    ai::PartyMemberAI partyMemberAi_;
    FindPlayerFn findPlayer_;
    FindAiCompanionsFn findAiCompanions_;
    BroadcastFn broadcast_;
    PersistStateFn persistState_;
    combat::CombatInstance::Rng rng_;
    std::unordered_map<uint32_t, ActiveCombat> combats_;
    std::unordered_map<std::string, uint32_t> characterCombatIds_;
    uint32_t nextCombatId_ = 1;
};

} // namespace tbeq::server
