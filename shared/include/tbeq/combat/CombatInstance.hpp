#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "tbeq/combat/CombatTypes.hpp"
#include "tbeq/content/MobCatalog.hpp"
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
        uint16_t level,
        CharacterState& state);
    void addMob(const content::MobDef& mobDef);

    void rollInitiative();
    bool submitAction(uint32_t actorSlot, CombatActionType action, uint32_t targetSlot);
    CombatActionType defaultPlayerAction(uint32_t actorSlot) const;
    CombatActionType chooseEnemyAction(uint32_t actorSlot) const;
    uint32_t chooseEnemyTarget(uint32_t actorSlot) const;

    std::vector<CombatEvent> takePendingEvents();
    std::vector<CombatLootRoll> rollLoot() const;
    void evaluateOutcome();

private:
    bool resolveMeleeAttack(CombatParticipant& actor, CombatParticipant& target);
    bool resolveFlee(CombatParticipant& actor);
    void applyDeath(CombatParticipant& participant);
    void checkOutcome();
    void advanceTurn();
    uint32_t allocateSlot();

    uint32_t combatId_ = 0;
    SkillResolver& skillResolver_;
    const content::MobCatalog& mobCatalog_;
    Rng& rng_;
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
