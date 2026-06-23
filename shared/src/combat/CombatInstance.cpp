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

bool hasUnlocked(const std::vector<std::string>& unlocked, const std::string& id)
{
    return std::find(unlocked.begin(), unlocked.end(), id) != unlocked.end();
}

} // namespace

uint16_t CombatParticipant::skillLevel(const std::string& skillId) const
{
    const auto it = skillLevels.find(skillId);
    if (it != skillLevels.end())
    {
        return it->second;
    }
    if (skillId == "offense")
    {
        return offenseSkillLevel;
    }
    if (skillId == "defense")
    {
        return defenseSkillLevel;
    }
    if (skillId == weaponSkillId)
    {
        return weaponSkillLevel;
    }
    return 1;
}

bool CombatParticipant::isStunned() const
{
    for (const auto& effect : statusEffects)
    {
        if (effect.type == StatusEffectType::Stun && effect.remainingTurns > 0)
        {
            return true;
        }
    }
    return false;
}

bool CombatParticipant::isSnared() const
{
    for (const auto& effect : statusEffects)
    {
        if (effect.type == StatusEffectType::Snare && effect.remainingTurns > 0)
        {
            return true;
        }
    }
    return false;
}

CombatInstance::CombatInstance(
    uint32_t combatId,
    SkillResolver& skillResolver,
    const content::MobCatalog& mobCatalog,
    const content::SpellCatalog& spellCatalog,
    const content::AbilityCatalog& abilityCatalog,
    Rng& rng)
    : combatId_(combatId)
    , skillResolver_(skillResolver)
    , mobCatalog_(mobCatalog)
    , spellCatalog_(spellCatalog)
    , abilityCatalog_(abilityCatalog)
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
    const std::string& classId,
    uint16_t level,
    CharacterState& state,
    bool playerControlled,
    bool aiCompanion)
{
    CombatParticipant participant;
    participant.slot = allocateSlot();
    participant.characterId = characterId;
    participant.name = name;
    participant.classId = classId;
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
    participant.channelingSkillLevel = state.skillLevel("channeling");
    for (const auto& [skillId, progress] : state.skills)
    {
        participant.skillLevels[skillId] = progress.level;
    }
    participant.unlockedSpells = state.unlockedSpells;
    participant.unlockedAbilities = state.unlockedAbilities;
    participant.isPlayerControlled = playerControlled;
    participant.isAiCompanion = aiCompanion;
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

    if (CombatParticipant* actor = currentActor())
    {
        processTurnStartEffects(*actor);
        CombatEvent event;
        event.type = CombatEventType::TurnStart;
        event.actorSlot = actor->slot;
        event.message = actor->name + " begins their turn.";
        pendingEvents_.push_back(std::move(event));
    }
}

bool CombatInstance::submitAction(const CombatActionIntent& intent)
{
    if (phase_ != CombatPhase::SelectAction || outcome_ != CombatOutcome::InProgress)
    {
        return false;
    }

    CombatParticipant* expected = currentActor();
    if (expected == nullptr)
    {
        return false;
    }

    CombatActionIntent resolvedIntent = intent;
    if (resolvedIntent.action == CombatActionType::None)
    {
        return false;
    }

    CombatParticipant* actor = expected;
    if (!actor->isAlive)
    {
        return false;
    }

    if (actor->isStunned())
    {
        CombatEvent event;
        event.type = CombatEventType::StatusApplied;
        event.actorSlot = actor->slot;
        event.message = actor->name + " is stunned and loses their turn.";
        pendingEvents_.push_back(std::move(event));
        advanceTurn();
        return true;
    }

    for (auto& participant : participants_)
    {
        participant.isDefending = false;
    }

    bool resolved = false;
    switch (resolvedIntent.action)
    {
    case CombatActionType::MeleeAttack:
    {
        CombatParticipant* target = participantBySlot(resolvedIntent.targetSlot);
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
        if (actor->isSnared())
        {
            CombatEvent event;
            event.type = CombatEventType::FleeFailed;
            event.actorSlot = actor->slot;
            event.message = actor->name + " is snared and cannot flee.";
            pendingEvents_.push_back(std::move(event));
            resolved = true;
            break;
        }
        resolved = resolveFlee(*actor);
        break;
    case CombatActionType::CastSpell:
    {
        CombatParticipant* target = participantBySlot(resolvedIntent.targetSlot);
        if (target == nullptr || !target->isAlive || resolvedIntent.spellId.empty())
        {
            return false;
        }
        resolved = resolveCastSpell(*actor, resolvedIntent.spellId, *target);
        break;
    }
    case CombatActionType::UseAbility:
    {
        CombatParticipant* target = participantBySlot(resolvedIntent.targetSlot);
        if (target == nullptr || !target->isAlive || resolvedIntent.abilityId.empty())
        {
            return false;
        }
        resolved = resolveUseAbility(*actor, resolvedIntent.abilityId, *target);
        break;
    }
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

bool CombatInstance::submitAction(uint32_t actorSlot, CombatActionType action, uint32_t targetSlot)
{
    CombatParticipant* expected = currentActor();
    if (expected == nullptr || expected->slot != actorSlot)
    {
        return false;
    }

    CombatActionIntent intent;
    intent.action = action;
    intent.targetSlot = targetSlot;
    return submitAction(intent);
}

CombatActionIntent CombatInstance::defaultPlayerAction(uint32_t actorSlot) const
{
    CombatActionIntent intent;
    const uint32_t target = chooseEnemyTarget(actorSlot);
    if (target == 0)
    {
        intent.action = CombatActionType::Defend;
        return intent;
    }
    intent.action = CombatActionType::MeleeAttack;
    intent.targetSlot = target;
    return intent;
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

bool CombatInstance::isValidSpellTarget(
    const CombatParticipant& actor,
    const content::SpellDef& spell,
    const CombatParticipant& target) const
{
    switch (spell.targetType)
    {
    case content::SpellTargetType::SingleAlly:
        return target.side == actor.side;
    case content::SpellTargetType::Self:
        return target.slot == actor.slot;
    case content::SpellTargetType::SingleEnemy:
        return target.side != actor.side;
    default:
        return false;
    }
}

bool CombatInstance::isValidAbilityTarget(
    const CombatParticipant& actor,
    const content::AbilityDef& ability,
    const CombatParticipant& target) const
{
    switch (ability.targetType)
    {
    case content::AbilityTargetType::Self:
        return target.slot == actor.slot;
    case content::AbilityTargetType::SingleEnemy:
        return target.side != actor.side;
    default:
        return false;
    }
}

void CombatInstance::applyStatusEffect(
    CombatParticipant& target,
    StatusEffectType type,
    uint16_t turns,
    int32_t tickValue,
    const std::string& sourceName)
{
    StatusEffect effect;
    effect.type = type;
    effect.remainingTurns = turns;
    effect.tickValue = tickValue;
    effect.sourceName = sourceName;
    target.statusEffects.push_back(std::move(effect));

    CombatEvent event;
    event.type = CombatEventType::StatusApplied;
    event.targetSlot = target.slot;
    event.value = static_cast<int32_t>(turns);
    switch (type)
    {
    case StatusEffectType::Stun:
        event.detail = "stun";
        event.message = target.name + " is stunned!";
        break;
    case StatusEffectType::Snare:
        event.detail = "snare";
        event.message = target.name + " is snared!";
        break;
    case StatusEffectType::Dot:
        event.detail = "dot";
        event.message = target.name + " is burning!";
        break;
    }
    pendingEvents_.push_back(std::move(event));
}

void CombatInstance::tickStatusEffects(CombatParticipant& participant)
{
    for (auto& effect : participant.statusEffects)
    {
        if (effect.type != StatusEffectType::Dot || effect.remainingTurns == 0)
        {
            continue;
        }

        if (!participant.godMode)
        {
            participant.hp = static_cast<uint16_t>(std::max(
                0,
                static_cast<int32_t>(participant.hp) - effect.tickValue));
        }

        CombatEvent event;
        event.type = CombatEventType::StatusTick;
        event.targetSlot = participant.slot;
        event.value = effect.tickValue;
        event.detail = "dot";
        event.message = participant.name + " takes " + std::to_string(effect.tickValue) + " damage from a DoT.";
        pendingEvents_.push_back(std::move(event));

        if (participant.hp == 0)
        {
            applyDeath(participant);
        }
    }

    participant.statusEffects.erase(
        std::remove_if(
            participant.statusEffects.begin(),
            participant.statusEffects.end(),
            [](const StatusEffect& effect)
            {
                return effect.remainingTurns == 0;
            }),
        participant.statusEffects.end());
}

void CombatInstance::processTurnStartEffects(CombatParticipant& participant)
{
    tickStatusEffects(participant);

    for (auto& effect : participant.statusEffects)
    {
        if (effect.remainingTurns > 0)
        {
            --effect.remainingTurns;
        }
    }
}

bool CombatInstance::resolveCastSpell(
    CombatParticipant& actor,
    const std::string& spellId,
    CombatParticipant& target)
{
    const content::SpellDef* spell = spellCatalog_.findSpell(spellId);
    if (spell == nullptr || !hasUnlocked(actor.unlockedSpells, spellId))
    {
        return false;
    }
    if (!isValidSpellTarget(actor, *spell, target))
    {
        return false;
    }
    if (actor.mana < spell->manaCost)
    {
        return false;
    }

    if (!skillResolver_.rollChanneling(actor.channelingSkillLevel, rng_))
    {
        CombatEvent fizzle;
        fizzle.type = CombatEventType::SpellFizzle;
        fizzle.actorSlot = actor.slot;
        fizzle.detail = spellId;
        fizzle.message = actor.name + "'s " + spell->name + " fizzles!";
        pendingEvents_.push_back(std::move(fizzle));
        return true;
    }

    actor.mana = static_cast<uint16_t>(actor.mana - spell->manaCost);
    CombatEvent manaEvent;
    manaEvent.type = CombatEventType::ManaSpend;
    manaEvent.actorSlot = actor.slot;
    manaEvent.value = static_cast<int32_t>(spell->manaCost);
    manaEvent.message = actor.name + " spends " + std::to_string(spell->manaCost) + " mana.";
    pendingEvents_.push_back(std::move(manaEvent));

    const uint16_t schoolSkill = actor.skillLevel(spell->linkedSkill);

    if (spell->effect == content::SpellEffectType::Heal)
    {
        const int32_t healAmount = skillResolver_.calculateHealAmount(spell->baseValue, schoolSkill, actor.level);
        target.hp = static_cast<uint16_t>(std::min(
            static_cast<int32_t>(target.maxHp),
            static_cast<int32_t>(target.hp) + healAmount));

        CombatEvent event;
        event.type = CombatEventType::Heal;
        event.actorSlot = actor.slot;
        event.targetSlot = target.slot;
        event.value = healAmount;
        event.detail = spellId;
        event.message = actor.name + " heals " + target.name + " for " + std::to_string(healAmount) + ".";
        pendingEvents_.push_back(std::move(event));
    }
    else
    {
        if (skillResolver_.rollSpellResist(schoolSkill, target.level, rng_))
        {
            CombatEvent resist;
            resist.type = CombatEventType::SpellResist;
            resist.actorSlot = actor.slot;
            resist.targetSlot = target.slot;
            resist.detail = spellId;
            resist.message = target.name + " resists " + actor.name + "'s " + spell->name + ".";
            pendingEvents_.push_back(std::move(resist));
            return true;
        }

        const int32_t damage = skillResolver_.calculateSpellDamage(spell->baseValue, schoolSkill, actor.level);
        if (!target.godMode)
        {
            target.hp = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(target.hp) - damage));
        }

        CombatEvent event;
        event.type = CombatEventType::SpellCast;
        event.actorSlot = actor.slot;
        event.targetSlot = target.slot;
        event.value = damage;
        event.detail = spellId;
        event.message = actor.name + " casts " + spell->name + " on " + target.name + " for " + std::to_string(damage) + " damage.";
        pendingEvents_.push_back(std::move(event));

        if (spell->effect == content::SpellEffectType::Dot && spell->dotTurns > 0)
        {
            applyStatusEffect(target, StatusEffectType::Dot, spell->dotTurns, damage / 3, actor.name);
        }

        if (target.hp == 0)
        {
            applyDeath(target);
        }
    }

    checkOutcome();
    return true;
}

bool CombatInstance::resolveUseAbility(
    CombatParticipant& actor,
    const std::string& abilityId,
    CombatParticipant& target)
{
    const content::AbilityDef* ability = abilityCatalog_.findAbility(abilityId);
    if (ability == nullptr || !hasUnlocked(actor.unlockedAbilities, abilityId))
    {
        return false;
    }
    if (!isValidAbilityTarget(actor, *ability, target))
    {
        return false;
    }

    const uint16_t maneuverSkill = actor.skillLevel(ability->skillCheck);

    const int32_t damage = skillResolver_.calculateAbilityDamage(
        ability->baseValue,
        actor.skillLevel(ability->linkedSkill),
        actor.offenseSkillLevel,
        actor.level);

    if (!target.godMode)
    {
        target.hp = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(target.hp) - damage));
    }

    CombatEvent event;
    event.type = CombatEventType::AbilityUsed;
    event.actorSlot = actor.slot;
    event.targetSlot = target.slot;
    event.value = damage;
    event.detail = abilityId;
    event.message = actor.name + " uses " + ability->name + " on " + target.name + " for " + std::to_string(damage) + " damage.";
    pendingEvents_.push_back(std::move(event));

    if (ability->effect == content::AbilityEffectType::DamageStun
        || ability->effect == content::AbilityEffectType::DamageSnare)
    {
        if (skillResolver_.rollManeuver(maneuverSkill, target.level, rng_))
        {
            if (ability->effect == content::AbilityEffectType::DamageStun && ability->stunTurns > 0)
            {
                applyStatusEffect(target, StatusEffectType::Stun, ability->stunTurns, 0, actor.name);
            }
            else if (ability->effect == content::AbilityEffectType::DamageSnare && ability->snareTurns > 0)
            {
                applyStatusEffect(target, StatusEffectType::Snare, ability->snareTurns, 0, actor.name);
            }
        }
    }

    if (target.hp == 0)
    {
        applyDeath(target);
    }

    checkOutcome();
    return true;
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
        if (CombatParticipant* actor = currentActor(); actor != nullptr && actor->isAlive)
        {
            processTurnStartEffects(*actor);
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
