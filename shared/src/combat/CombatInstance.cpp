#include "tbeq/combat/CombatInstance.hpp"

#include <algorithm>
#include <cmath>

namespace tbeq::combat
{

namespace
{

int32_t rollRange(std::mt19937& rng, int32_t minValue, int32_t maxValue)
{
    std::uniform_int_distribution<int32_t> dist(minValue, maxValue);
    return dist(rng);
}

} // namespace

CombatInstance::CombatInstance(
    uint32_t combatId,
    SkillResolver& skillResolver,
    const content::MobCatalog& mobCatalog,
    Rng& rng)
    : combatId_(combatId)
    , skillResolver_(skillResolver)
    , mobCatalog_(mobCatalog)
    , rng_(rng)
{
}

CombatParticipant* CombatInstance::participantBySlot(uint32_t slot)
{
    for (auto& participant : participants_)
    {
        if (participant.slot == slot)
        {
            return &participant;
        }
    }
    return nullptr;
}

const CombatParticipant* CombatInstance::participantBySlot(uint32_t slot) const
{
    for (const auto& participant : participants_)
    {
        if (participant.slot == slot)
        {
            return &participant;
        }
    }
    return nullptr;
}

CombatParticipant* CombatInstance::currentActor()
{
    if (currentTurnIndex_ >= turnOrder_.size())
    {
        return nullptr;
    }
    return participantBySlot(turnOrder_[currentTurnIndex_]);
}

const CombatParticipant* CombatInstance::currentActor() const
{
    if (currentTurnIndex_ >= turnOrder_.size())
    {
        return nullptr;
    }
    return participantBySlot(turnOrder_[currentTurnIndex_]);
}

uint32_t CombatInstance::allocateSlot()
{
    return nextSlot_++;
}

void CombatInstance::addPlayer(
    const std::string& characterId,
    const std::string& name,
    uint16_t level,
    CharacterState& state)
{
    CombatParticipant participant;
    participant.slot = allocateSlot();
    participant.characterId = characterId;
    participant.name = name;
    participant.side = CombatSide::Player;
    participant.level = level;
    participant.hp = state.hp;
    participant.maxHp = state.maxHp;
    participant.mana = state.mana;
    participant.maxMana = state.maxMana;
    participant.agi = state.agi;
    participant.weaponSkillId = state.equippedWeaponSkill;
    participant.weaponSkillLevel = state.skillLevel(state.equippedWeaponSkill);
    participant.offenseSkillLevel = state.skillLevel("offense");
    participant.defenseSkillLevel = state.skillLevel("defense");
    participant.isPlayerControlled = true;
    participant.godMode = state.godMode;
    participants_.push_back(std::move(participant));
}

void CombatInstance::addMob(const content::MobDef& mobDef)
{
    CombatParticipant participant;
    participant.slot = allocateSlot();
    participant.mobId = mobDef.id;
    participant.name = mobDef.name;
    participant.side = CombatSide::Enemy;
    participant.level = mobDef.level;
    participant.hp = mobDef.hp;
    participant.maxHp = mobDef.hp;
    participant.offense = mobDef.offense;
    participant.defense = mobDef.defense;
    participant.agi = mobDef.agi;
    participant.isPlayerControlled = false;
    participants_.push_back(std::move(participant));
}

void CombatInstance::rollInitiative()
{
    struct InitiativeEntry
    {
        uint32_t slot = 0;
        int32_t score = 0;
    };

    std::vector<InitiativeEntry> entries;
    entries.reserve(participants_.size());
    for (const auto& participant : participants_)
    {
        if (!participant.isAlive)
        {
            continue;
        }
        InitiativeEntry entry;
        entry.slot = participant.slot;
        entry.score = static_cast<int32_t>(participant.agi) + rollRange(rng_, 1, 100);
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const InitiativeEntry& a, const InitiativeEntry& b)
    {
        return a.score > b.score;
    });

    turnOrder_.clear();
    for (const auto& entry : entries)
    {
        turnOrder_.push_back(entry.slot);
    }
    currentTurnIndex_ = 0;
    phase_ = CombatPhase::SelectAction;

    if (const auto* actor = currentActor())
    {
        CombatEvent event;
        event.type = CombatEventType::TurnStart;
        event.actorSlot = actor->slot;
        event.message = actor->name + " begins their turn.";
        pendingEvents_.push_back(std::move(event));
    }
}

bool CombatInstance::submitAction(uint32_t actorSlot, CombatActionType action, uint32_t targetSlot)
{
    if (phase_ != CombatPhase::SelectAction || outcome_ != CombatOutcome::InProgress)
    {
        return false;
    }

    CombatParticipant* actor = participantBySlot(actorSlot);
    if (actor == nullptr || !actor->isAlive)
    {
        return false;
    }

    const CombatParticipant* expected = currentActor();
    if (expected == nullptr || expected->slot != actorSlot)
    {
        return false;
    }

    for (auto& participant : participants_)
    {
        participant.isDefending = false;
    }

    bool resolved = false;
    switch (action)
    {
    case CombatActionType::MeleeAttack:
    {
        CombatParticipant* target = participantBySlot(targetSlot);
        if (target == nullptr || !target->isAlive || target->side == actor->side)
        {
            return false;
        }
        resolved = resolveMeleeAttack(*actor, *target);
        break;
    }
    case CombatActionType::Defend:
    {
        actor->isDefending = true;
        CombatEvent event;
        event.type = CombatEventType::Defend;
        event.actorSlot = actor->slot;
        event.message = actor->name + " takes a defensive stance.";
        pendingEvents_.push_back(std::move(event));
        resolved = true;
        break;
    }
    case CombatActionType::Flee:
        resolved = resolveFlee(*actor);
        break;
    default:
        return false;
    }

    if (!resolved)
    {
        return false;
    }

    if (outcome_ == CombatOutcome::InProgress)
    {
        advanceTurn();
    }
    return true;
}

CombatActionType CombatInstance::defaultPlayerAction(uint32_t actorSlot) const
{
    const uint32_t target = chooseEnemyTarget(actorSlot);
    if (target == 0)
    {
        return CombatActionType::Defend;
    }
    (void)target;
    return CombatActionType::MeleeAttack;
}

CombatActionType CombatInstance::chooseEnemyAction(uint32_t actorSlot) const
{
    (void)actorSlot;
    if (chooseEnemyTarget(actorSlot) == 0)
    {
        return CombatActionType::Defend;
    }
    return CombatActionType::MeleeAttack;
}

uint32_t CombatInstance::chooseEnemyTarget(uint32_t actorSlot) const
{
    const CombatParticipant* actor = participantBySlot(actorSlot);
    if (actor == nullptr)
    {
        return 0;
    }

    std::vector<const CombatParticipant*> candidates;
    for (const auto& participant : participants_)
    {
        if (!participant.isAlive || participant.side == actor->side)
        {
            continue;
        }
        candidates.push_back(&participant);
    }

    if (candidates.empty())
    {
        return 0;
    }

    std::sort(candidates.begin(), candidates.end(), [](const CombatParticipant* a, const CombatParticipant* b)
    {
        if (a->hp != b->hp)
        {
            return a->hp < b->hp;
        }
        return a->slot < b->slot;
    });

    if (rollRange(rng_, 1, 100) <= 70)
    {
        return candidates.front()->slot;
    }

    const size_t index = static_cast<size_t>(rollRange(rng_, 0, static_cast<int32_t>(candidates.size()) - 1));
    return candidates[index]->slot;
}

bool CombatInstance::resolveMeleeAttack(CombatParticipant& actor, CombatParticipant& target)
{
    const auto hit = skillResolver_.rollMeleeHit(
        actor.offenseSkillLevel,
        actor.weaponSkillLevel,
        target.defenseSkillLevel > 0 ? target.defenseSkillLevel : target.defense,
        rng_);

    if (!hit)
    {
        CombatEvent event;
        event.type = CombatEventType::Miss;
        event.actorSlot = actor.slot;
        event.targetSlot = target.slot;
        event.message = actor.name + " misses " + target.name + ".";
        pendingEvents_.push_back(std::move(event));
        return true;
    }

    const uint16_t targetDefense = target.defenseSkillLevel > 0 ? target.defenseSkillLevel : target.defense;
    int32_t damage = skillResolver_.calculateMeleeDamage(
        actor.level,
        actor.weaponSkillLevel,
        actor.offenseSkillLevel,
        targetDefense,
        rng_);

    if (target.isDefending)
    {
        damage = std::max(1, damage / 2);
    }

    if (!target.godMode)
    {
        target.hp = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(target.hp) - damage));
    }

    CombatEvent event;
    event.type = CombatEventType::Damage;
    event.actorSlot = actor.slot;
    event.targetSlot = target.slot;
    event.value = damage;
    event.message = actor.name + " hits " + target.name + " for " + std::to_string(damage) + " damage.";
    pendingEvents_.push_back(std::move(event));

    if (target.hp == 0)
    {
        applyDeath(target);
    }

    checkOutcome();
    return true;
}

bool CombatInstance::resolveFlee(CombatParticipant& actor)
{
    const int32_t livingEnemies = static_cast<int32_t>(std::count_if(
        participants_.begin(),
        participants_.end(),
        [](const CombatParticipant& participant)
        {
            return participant.isAlive && participant.side == CombatSide::Enemy;
        }));

    const bool fled = skillResolver_.rollFlee(
        actor.defenseSkillLevel,
        actor.agi,
        livingEnemies,
        rng_);

    CombatEvent attempt;
    attempt.type = fled ? CombatEventType::FleeSuccess : CombatEventType::FleeFailed;
    attempt.actorSlot = actor.slot;
    attempt.message = fled
        ? actor.name + " escapes from combat!"
        : actor.name + " fails to flee.";
    pendingEvents_.push_back(std::move(attempt));

    if (fled)
    {
        outcome_ = CombatOutcome::Fled;
        phase_ = CombatPhase::Ended;
        CombatEvent endEvent;
        endEvent.type = CombatEventType::CombatEnd;
        endEvent.detail = "fled";
        endEvent.message = "Combat ended: party fled.";
        pendingEvents_.push_back(std::move(endEvent));
    }

    return true;
}

void CombatInstance::applyDeath(CombatParticipant& participant)
{
    participant.isAlive = false;
    CombatEvent event;
    event.type = CombatEventType::Death;
    event.actorSlot = participant.slot;
    event.message = participant.name + " has been slain.";
    pendingEvents_.push_back(std::move(event));
}

void CombatInstance::evaluateOutcome()
{
    checkOutcome();
}

void CombatInstance::checkOutcome()
{
    bool anyPlayerAlive = false;
    bool anyEnemyAlive = false;
    for (const auto& participant : participants_)
    {
        if (!participant.isAlive)
        {
            continue;
        }
        if (participant.side == CombatSide::Player)
        {
            anyPlayerAlive = true;
        }
        else
        {
            anyEnemyAlive = true;
        }
    }

    if (!anyPlayerAlive)
    {
        outcome_ = CombatOutcome::Defeat;
        phase_ = CombatPhase::Ended;
        CombatEvent event;
        event.type = CombatEventType::CombatEnd;
        event.detail = "defeat";
        event.message = "Combat ended: party defeated.";
        pendingEvents_.push_back(std::move(event));
        return;
    }

    if (!anyEnemyAlive)
    {
        outcome_ = CombatOutcome::Victory;
        phase_ = CombatPhase::Ended;
        CombatEvent event;
        event.type = CombatEventType::CombatEnd;
        event.detail = "victory";
        event.message = "Combat ended: enemies defeated.";
        pendingEvents_.push_back(std::move(event));
    }
}

void CombatInstance::advanceTurn()
{
    if (outcome_ != CombatOutcome::InProgress)
    {
        return;
    }

    const size_t startIndex = currentTurnIndex_;
    do
    {
        currentTurnIndex_ = (currentTurnIndex_ + 1) % turnOrder_.size();
        if (const auto* actor = currentActor(); actor != nullptr && actor->isAlive)
        {
            CombatEvent event;
            event.type = CombatEventType::TurnStart;
            event.actorSlot = actor->slot;
            event.message = actor->name + " begins their turn.";
            pendingEvents_.push_back(std::move(event));
            return;
        }
    }
    while (currentTurnIndex_ != startIndex);

    phase_ = CombatPhase::Ended;
}

std::vector<CombatEvent> CombatInstance::takePendingEvents()
{
    std::vector<CombatEvent> events = std::move(pendingEvents_);
    pendingEvents_.clear();
    return events;
}

std::vector<CombatLootRoll> CombatInstance::rollLoot() const
{
    std::vector<CombatLootRoll> loot;
    for (const auto& participant : participants_)
    {
        if (participant.side != CombatSide::Enemy || participant.isAlive)
        {
            continue;
        }

        const content::MobDef* mobDef = mobCatalog_.findMob(participant.mobId);
        if (mobDef == nullptr)
        {
            continue;
        }

        for (const auto& entry : mobDef->loot)
        {
            const int32_t roll = rollRange(const_cast<Rng&>(rng_), 1, 100);
            if (roll > entry.dropChance)
            {
                continue;
            }

            CombatLootRoll lootRoll;
            lootRoll.itemId = entry.itemId;
            lootRoll.quantity = static_cast<uint16_t>(rollRange(
                const_cast<Rng&>(rng_),
                entry.minQuantity,
                entry.maxQuantity));
            loot.push_back(std::move(lootRoll));
        }
    }
    return loot;
}

} // namespace tbeq::combat
