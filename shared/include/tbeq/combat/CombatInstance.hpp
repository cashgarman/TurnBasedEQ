#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "tbeq/combat/BossScriptCatalog.hpp"
#include "tbeq/combat/CombatTypes.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/MobCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"
#include "tbeq/core/CharacterState.hpp"
#include "tbeq/skills/SkillResolver.hpp"

namespace tbeq::combat
{

struct CombatLootRoll
{
    std::string itemId;
    uint16_t quantity = 1;
};

class CombatInstance
{
public:
    using Rng = std::mt19937;

    CombatInstance(
        uint32_t combatId,
        SkillResolver& skillResolver,
        const content::MobCatalog& mobCatalog,
        const content::SpellCatalog& spellCatalog,
        const content::AbilityCatalog& abilityCatalog,
        const BossScriptCatalog& bossScripts,
        Rng& rng);

    uint32_t combatId() const { return combatId_; }
    CombatPhase phase() const { return phase_; }
    CombatOutcome outcome() const { return outcome_; }
    const std::vector<CombatParticipant>& participants() const { return participants_; }
    const std::vector<uint32_t>& turnOrder() const { return turnOrder_; }
    size_t currentTurnIndex() const { return currentTurnIndex_; }
    uint32_t turnDurationMs() const { return turnDurationMs_; }

    CombatParticipant* participantBySlot(uint32_t slot);
    const CombatParticipant* participantBySlot(uint32_t slot) const;
    CombatParticipant* currentActor();
    const CombatParticipant* currentActor() const;

    void addPlayer(
        const std::string& characterId,
        const std::string& name,
        const std::string& classId,
        const std::string& raceId,
        uint16_t level,
        CharacterState& state,
        bool playerControlled,
        bool aiCompanion);
    void addMob(const content::MobDef& mobDef);

    void rollInitiative();
    bool submitAction(const CombatActionIntent& intent);
    bool submitAction(uint32_t actorSlot, CombatActionType action, uint32_t targetSlot);
    CombatActionIntent defaultPlayerAction(uint32_t actorSlot) const;
    CombatActionType chooseEnemyAction(uint32_t actorSlot) const;
    uint32_t chooseEnemyTarget(uint32_t actorSlot) const;

    std::vector<CombatEvent> takePendingEvents();
    std::vector<CombatLootRoll> rollLoot() const;
    void evaluateOutcome();
    uint32_t mobExperienceReward() const;
    void addThreat(uint32_t playerSlot, int32_t amount);
    void applyTaunt(uint32_t playerSlot);

private:
    bool resolveMeleeAttack(CombatParticipant& actor, CombatParticipant& target);
    bool resolveCastSpell(CombatParticipant& actor, const std::string& spellId, CombatParticipant& target);
    bool resolveUseAbility(CombatParticipant& actor, const std::string& abilityId, CombatParticipant& target);
    bool resolveFlee(CombatParticipant& actor);
    void applyDeath(CombatParticipant& participant);
    void checkOutcome();
    void advanceTurn();
    void tickStatusEffects(CombatParticipant& participant);
    void processTurnStartEffects(CombatParticipant& participant);
    bool isValidSpellTarget(const CombatParticipant& actor, const content::SpellDef& spell, const CombatParticipant& target) const;
    bool isValidAbilityTarget(const CombatParticipant& actor, const content::AbilityDef& ability, const CombatParticipant& target) const;
    void applyStatusEffect(CombatParticipant& target, StatusEffectType type, uint16_t turns, int32_t tickValue, const std::string& sourceName);
    uint32_t allocateSlot();
    void updateBossPhase(CombatParticipant& boss);
    const BossPhaseDef* activeBossPhase(const CombatParticipant& boss) const;
    uint32_t chooseThreatTarget(uint32_t actorSlot, BossTargetMode mode) const;

    uint32_t combatId_ = 0;
    SkillResolver& skillResolver_;
    const content::MobCatalog& mobCatalog_;
    const content::SpellCatalog& spellCatalog_;
    const content::AbilityCatalog& abilityCatalog_;
    const BossScriptCatalog& bossScripts_;
    Rng& rng_;
    std::unordered_map<uint32_t, int32_t> threatByPlayerSlot_;
    std::vector<CombatParticipant> participants_;
    std::vector<uint32_t> turnOrder_;
    size_t currentTurnIndex_ = 0;
    CombatPhase phase_ = CombatPhase::SelectAction;
    CombatOutcome outcome_ = CombatOutcome::InProgress;
    std::vector<CombatEvent> pendingEvents_;
    uint32_t turnDurationMs_ = 30000;
    uint32_t nextSlot_ = 1;
};

} // namespace tbeq::combat
