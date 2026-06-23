#include "combat/CombatManager.hpp"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "tbeq/net/PacketSerializer.hpp"

namespace tbeq::server
{

CombatManager::CombatManager(
    asio::io_context& io,
    SkillResolver& skillResolver,
    const content::MobCatalog& mobCatalog,
    FindPlayerFn findPlayer,
    BroadcastFn broadcast,
    PersistStateFn persistState)
    : io_(io)
    , skillResolver_(skillResolver)
    , mobCatalog_(mobCatalog)
    , findPlayer_(std::move(findPlayer))
    , broadcast_(std::move(broadcast))
    , persistState_(std::move(persistState))
    , rng_(std::random_device{}())
{
}

uint32_t CombatManager::allocateCombatId()
{
    return nextCombatId_++;
}

net::CombatParticipantPayload CombatManager::toPayload(const combat::CombatParticipant& participant) const
{
    net::CombatParticipantPayload payload;
    payload.combatSlot = participant.slot;
    payload.name = participant.name;
    payload.side = static_cast<uint8_t>(participant.side);
    payload.hp = participant.hp;
    payload.maxHp = participant.maxHp;
    payload.mana = participant.mana;
    payload.maxMana = participant.maxMana;
    payload.isAlive = participant.isAlive;
    payload.isPlayerControlled = participant.isPlayerControlled;
    return payload;
}

net::CombatStartPayload CombatManager::buildStartPayload(const combat::CombatInstance& instance) const
{
    net::CombatStartPayload payload;
    payload.combatId = instance.combatId();
    payload.currentTurnIndex = static_cast<uint32_t>(instance.currentTurnIndex());
    payload.turnDurationMs = instance.turnDurationMs();
    if (const auto* actor = instance.currentActor())
    {
        payload.currentActorSlot = actor->slot;
    }
    for (const auto& participant : instance.participants())
    {
        payload.participants.push_back(toPayload(participant));
    }
    payload.turnOrder = instance.turnOrder();
    return payload;
}

net::CombatUpdatePayload CombatManager::buildUpdatePayload(const combat::CombatInstance& instance) const
{
    net::CombatUpdatePayload payload;
    payload.combatId = instance.combatId();
    payload.currentTurnIndex = static_cast<uint32_t>(instance.currentTurnIndex());
    payload.turnDurationMs = instance.turnDurationMs();
    if (const auto* actor = instance.currentActor())
    {
        payload.currentActorSlot = actor->slot;
    }
    for (const auto& participant : instance.participants())
    {
        payload.participants.push_back(toPayload(participant));
    }
    return payload;
}

net::CombatEventPayload CombatManager::toEventPayload(uint32_t combatId, const combat::CombatEvent& event) const
{
    net::CombatEventPayload payload;
    payload.combatId = combatId;
    payload.eventType = static_cast<uint8_t>(event.type);
    payload.actorSlot = event.actorSlot;
    payload.targetSlot = event.targetSlot;
    payload.value = event.value;
    payload.message = event.message;
    payload.detail = event.detail;
    return payload;
}

void CombatManager::syncParticipantFromPlayer(combat::CombatParticipant& participant, const PlayerView& player)
{
    if (player.state == nullptr)
    {
        return;
    }

    participant.hp = player.state->hp;
    participant.maxHp = player.state->maxHp;
    participant.mana = player.state->mana;
    participant.maxMana = player.state->maxMana;
    participant.agi = player.state->agi;
    participant.weaponSkillId = player.state->equippedWeaponSkill;
    participant.weaponSkillLevel = player.state->skillLevel(player.state->equippedWeaponSkill);
    participant.offenseSkillLevel = player.state->skillLevel("offense");
    participant.defenseSkillLevel = player.state->skillLevel("defense");
    participant.godMode = player.state->godMode;
}

void CombatManager::syncPlayerFromParticipant(const combat::CombatParticipant& participant, PlayerView& player)
{
    if (player.state == nullptr)
    {
        return;
    }

    player.state->hp = participant.hp;
    player.state->maxHp = participant.maxHp;
    player.state->mana = participant.mana;
    player.state->maxMana = participant.maxMana;
    if (player.combatSlot != nullptr)
    {
        *player.combatSlot = participant.slot;
    }
}

void CombatManager::applySkillXp(PlayerView& player, bool hit)
{
    if (player.state == nullptr)
    {
        return;
    }

    const uint32_t xpGain = skillResolver_.combatSkillXpGain(hit);
    const uint16_t cap = skillResolver_.getCap(player.classId, "offense", player.level);

    const auto applyOne = [&](const std::string& skillId)
    {
        auto& progress = player.state->skillOrDefault(skillId);
        const uint16_t oldLevel = progress.level;
        skillResolver_.applyGain(progress, xpGain, cap);
        if (progress.level > oldLevel)
        {
            net::SkillGainPayload gain;
            gain.skillId = skillId;
            gain.oldLevel = oldLevel;
            gain.newLevel = progress.level;
            gain.message = "Your " + skillId + " skill has increased! (" + std::to_string(oldLevel)
                + " -> " + std::to_string(progress.level) + ")";
            broadcast_({player.characterId}, net::ClientPacketType::SkillGain, net::serialize(gain));
        }
    };

    applyOne("offense");
    applyOne(player.state->equippedWeaponSkill);
}

void CombatManager::grantLoot(PlayerView& player, const std::vector<combat::CombatLootRoll>& loot)
{
    if (player.state == nullptr)
    {
        return;
    }

    for (const auto& roll : loot)
    {
        player.state->addItem(roll.itemId, roll.quantity);
        net::CombatEventPayload event;
        event.combatId = player.combatId != nullptr ? *player.combatId : 0;
        event.eventType = static_cast<uint8_t>(combat::CombatEventType::Loot);
        event.detail = roll.itemId;
        event.value = roll.quantity;
        event.message = "You receive " + std::to_string(roll.quantity) + " x " + roll.itemId + ".";
        broadcast_({player.characterId}, net::ClientPacketType::CombatEvent, net::serialize(event));
    }
}

CombatManager::ActiveCombat* CombatManager::findCombatForCharacter(const std::string& characterId)
{
    const auto it = characterCombatIds_.find(characterId);
    if (it == characterCombatIds_.end())
    {
        return nullptr;
    }
    const auto combatIt = combats_.find(it->second);
    if (combatIt == combats_.end())
    {
        return nullptr;
    }
    return &combatIt->second;
}

const CombatManager::ActiveCombat* CombatManager::findCombatForCharacter(const std::string& characterId) const
{
    const auto it = characterCombatIds_.find(characterId);
    if (it == characterCombatIds_.end())
    {
        return nullptr;
    }
    const auto combatIt = combats_.find(it->second);
    if (combatIt == combats_.end())
    {
        return nullptr;
    }
    return &combatIt->second;
}

bool CombatManager::isInCombat(const std::string& characterId) const
{
    return findCombatForCharacter(characterId) != nullptr;
}

bool CombatManager::tryStartSpawnCombat(PlayerView& player, const std::string& mobTable)
{
    if (player.inCombat != nullptr && *player.inCombat)
    {
        return false;
    }

    const auto mobIds = mobCatalog_.resolveMobTable(mobTable);
    if (mobIds.empty())
    {
        return false;
    }

    return startDebugCombat(player, mobIds);
}

bool CombatManager::startDebugCombat(PlayerView& player, const std::vector<std::string>& mobIds)
{
    if (player.inCombat != nullptr && *player.inCombat)
    {
        return false;
    }
    if (player.state == nullptr)
    {
        return false;
    }

    const uint32_t combatId = allocateCombatId();
    ActiveCombat activeCombat;
    activeCombat.instance = std::make_unique<combat::CombatInstance>(combatId, skillResolver_, mobCatalog_, rng_);
    activeCombat.playerCharacterIds.push_back(player.characterId);

    activeCombat.instance->addPlayer(player.characterId, player.name, player.level, *player.state);
    for (const auto& participant : activeCombat.instance->participants())
    {
        if (participant.isPlayerControlled)
        {
            activeCombat.slotToCharacterId[participant.slot] = player.characterId;
            if (player.combatSlot != nullptr)
            {
                *player.combatSlot = participant.slot;
            }
            break;
        }
    }

    for (const auto& mobId : mobIds)
    {
        const content::MobDef* mobDef = mobCatalog_.findMob(mobId);
        if (mobDef == nullptr)
        {
            continue;
        }
        activeCombat.instance->addMob(*mobDef);
    }

    if (activeCombat.instance->participants().size() < 2)
    {
        return false;
    }

    activeCombat.instance->rollInitiative();
    combats_.emplace(combatId, std::move(activeCombat));
    characterCombatIds_[player.characterId] = combatId;

    if (player.inCombat != nullptr)
    {
        *player.inCombat = true;
    }
    if (player.combatId != nullptr)
    {
        *player.combatId = combatId;
    }

    auto& combat = combats_.at(combatId);
    broadcastCombatStart(combat);
    beginTurn(combat);
    return true;
}

void CombatManager::broadcastCombatStart(ActiveCombat& combat)
{
    const auto payload = buildStartPayload(*combat.instance);
    broadcast_(combat.playerCharacterIds, net::ClientPacketType::CombatStart, net::serialize(payload));
}

void CombatManager::broadcastUpdate(ActiveCombat& combat)
{
    const auto payload = buildUpdatePayload(*combat.instance);
    broadcast_(combat.playerCharacterIds, net::ClientPacketType::CombatUpdate, net::serialize(payload));
}

void CombatManager::broadcastEvents(ActiveCombat& combat, const std::vector<combat::CombatEvent>& events)
{
    for (const auto& event : events)
    {
        const auto payload = toEventPayload(combat.instance->combatId(), event);
        broadcast_(combat.playerCharacterIds, net::ClientPacketType::CombatEvent, net::serialize(payload));

        if (event.type == combat::CombatEventType::Damage)
        {
            const auto slotIt = combat.slotToCharacterId.find(event.actorSlot);
            if (slotIt != combat.slotToCharacterId.end())
            {
                if (PlayerView* actor = findPlayer_(slotIt->second))
                {
                    applySkillXp(*actor, true);
                }
            }
        }
        else if (event.type == combat::CombatEventType::Miss)
        {
            const auto slotIt = combat.slotToCharacterId.find(event.actorSlot);
            if (slotIt != combat.slotToCharacterId.end())
            {
                if (PlayerView* actor = findPlayer_(slotIt->second))
                {
                    applySkillXp(*actor, false);
                }
            }
        }
    }
}

void CombatManager::beginTurn(ActiveCombat& combat)
{
    if (combat.instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(combat);
        return;
    }

    const combat::CombatParticipant* actor = combat.instance->currentActor();
    if (actor == nullptr)
    {
        finishCombat(combat);
        return;
    }

    broadcastUpdate(combat);

    if (!actor->isPlayerControlled)
    {
        resolveEnemyTurn(combat);
        return;
    }

    combat.turnTimer = std::make_shared<asio::steady_timer>(io_, std::chrono::milliseconds(combat.instance->turnDurationMs()));
    const uint32_t combatId = combat.instance->combatId();
    const uint32_t actorSlot = actor->slot;
    combat.turnTimer->async_wait([this, combatId, actorSlot](const std::error_code& ec)
    {
        if (ec)
        {
            return;
        }
        onTurnTimeout(combatId, actorSlot);
    });
}

void CombatManager::resolveEnemyTurn(ActiveCombat& combat)
{
    const combat::CombatParticipant* actor = combat.instance->currentActor();
    if (actor == nullptr)
    {
        return;
    }

    const auto action = combat.instance->chooseEnemyAction(actor->slot);
    uint32_t targetSlot = 0;
    if (action == combat::CombatActionType::MeleeAttack)
    {
        targetSlot = combat.instance->chooseEnemyTarget(actor->slot);
    }

    if (!combat.instance->submitAction(actor->slot, action, targetSlot))
    {
        combat.instance->submitAction(actor->slot, combat::CombatActionType::Defend, 0);
    }

    const auto events = combat.instance->takePendingEvents();
    broadcastEvents(combat, events);

    for (const auto& [slot, characterId] : combat.slotToCharacterId)
    {
        if (combat::CombatParticipant* participant = combat.instance->participantBySlot(slot))
        {
            if (PlayerView* player = findPlayer_(characterId))
            {
                syncPlayerFromParticipant(*participant, *player);
            }
        }
    }

    if (combat.instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(combat);
        return;
    }

    beginTurn(combat);
}

void CombatManager::onTurnTimeout(uint32_t combatId, uint32_t actorSlot)
{
    const auto combatIt = combats_.find(combatId);
    if (combatIt == combats_.end())
    {
        return;
    }

    ActiveCombat& combat = combatIt->second;
    const combat::CombatParticipant* actor = combat.instance->currentActor();
    if (actor == nullptr || actor->slot != actorSlot)
    {
        return;
    }

    const auto action = combat::CombatActionType::MeleeAttack;
    const uint32_t targetSlot = combat.instance->chooseEnemyTarget(actorSlot);
    if (targetSlot == 0)
    {
        combat.instance->submitAction(actorSlot, combat::CombatActionType::Defend, 0);
    }
    else
    {
        combat.instance->submitAction(actorSlot, action, targetSlot);
    }

    const auto events = combat.instance->takePendingEvents();
    broadcastEvents(combat, events);

    for (const auto& [slot, characterId] : combat.slotToCharacterId)
    {
        if (combat::CombatParticipant* participant = combat.instance->participantBySlot(slot))
        {
            if (PlayerView* player = findPlayer_(characterId))
            {
                syncPlayerFromParticipant(*participant, *player);
            }
        }
    }

    if (combat.instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(combat);
        return;
    }

    beginTurn(combat);
}

bool CombatManager::submitAction(
    const std::string& characterId,
    const net::SubmitActionPayload& request,
    net::SubmitActionResultPayload& result)
{
    ActiveCombat* combat = findCombatForCharacter(characterId);
    if (combat == nullptr || combat->instance->combatId() != request.combatId)
    {
        result.message = "Not in combat";
        return false;
    }

    const auto slotIt = std::find_if(
        combat->slotToCharacterId.begin(),
        combat->slotToCharacterId.end(),
        [&](const auto& entry)
        {
            return entry.second == characterId;
        });
    if (slotIt == combat->slotToCharacterId.end())
    {
        result.message = "Combat slot not found";
        return false;
    }

    if (combat->turnTimer)
    {
        combat->turnTimer->cancel();
        combat->turnTimer.reset();
    }

    const auto action = static_cast<combat::CombatActionType>(request.actionType);
    if (!combat->instance->submitAction(slotIt->first, action, request.targetCombatSlot))
    {
        result.message = "Invalid action";
        return false;
    }

    const auto events = combat->instance->takePendingEvents();
    broadcastEvents(*combat, events);

    for (const auto& [slot, boundCharacterId] : combat->slotToCharacterId)
    {
        if (combat::CombatParticipant* participant = combat->instance->participantBySlot(slot))
        {
            if (PlayerView* player = findPlayer_(boundCharacterId))
            {
                syncPlayerFromParticipant(*participant, *player);
            }
        }
    }

    result.ok = true;
    result.message = "Action accepted";

    if (combat->instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(*combat);
    }
    else
    {
        beginTurn(*combat);
    }

    return true;
}

void CombatManager::finishCombat(ActiveCombat& combat)
{
    if (combat.turnTimer)
    {
        combat.turnTimer->cancel();
        combat.turnTimer.reset();
    }

    const auto outcome = combat.instance->outcome();
    if (outcome == combat::CombatOutcome::Victory)
    {
        for (const auto& characterId : combat.playerCharacterIds)
        {
            if (PlayerView* player = findPlayer_(characterId))
            {
                const auto loot = combat.instance->rollLoot();
                grantLoot(*player, loot);
                if (player->state != nullptr)
                {
                    persistState_(characterId, *player->state);
                }

                net::CharacterVitalsPayload vitals;
                vitals.hp = player->state->hp;
                vitals.maxHp = player->state->maxHp;
                vitals.mana = player->state->mana;
                vitals.maxMana = player->state->maxMana;
                broadcast_({characterId}, net::ClientPacketType::CharacterVitals, net::serialize(vitals));
            }
        }
    }

    net::CombatEndPayload endPayload;
    endPayload.combatId = combat.instance->combatId();
    endPayload.result = static_cast<uint8_t>(outcome);
    switch (outcome)
    {
    case combat::CombatOutcome::Victory:
        endPayload.message = "Victory!";
        break;
    case combat::CombatOutcome::Defeat:
        endPayload.message = "You have been defeated.";
        break;
    case combat::CombatOutcome::Fled:
        endPayload.message = "You fled from combat.";
        break;
    default:
        endPayload.message = "Combat ended.";
        break;
    }
    broadcast_(combat.playerCharacterIds, net::ClientPacketType::CombatEnd, net::serialize(endPayload));

    for (const auto& characterId : combat.playerCharacterIds)
    {
        if (PlayerView* player = findPlayer_(characterId))
        {
            if (player->inCombat != nullptr)
            {
                *player->inCombat = false;
            }
            if (player->combatId != nullptr)
            {
                *player->combatId = 0;
            }
            if (player->combatSlot != nullptr)
            {
                *player->combatSlot = 0;
            }
            if (player->state != nullptr)
            {
                persistState_(characterId, *player->state);
            }
        }
        characterCombatIds_.erase(characterId);
    }

    combats_.erase(combat.instance->combatId());
}

bool CombatManager::forceEndCombat(const std::string& characterId, combat::CombatOutcome outcome)
{
    ActiveCombat* combat = findCombatForCharacter(characterId);
    if (combat == nullptr)
    {
        return false;
    }

    if (outcome == combat::CombatOutcome::Victory)
    {
        for (const auto& participant : combat->instance->participants())
        {
            if (participant.side == combat::CombatSide::Enemy && participant.isAlive)
            {
                if (combat::CombatParticipant* enemy = combat->instance->participantBySlot(participant.slot))
                {
                    enemy->hp = 0;
                    enemy->isAlive = false;
                }
            }
        }
    }
    else if (outcome == combat::CombatOutcome::Defeat)
    {
        for (const auto& participant : combat->instance->participants())
        {
            if (participant.side == combat::CombatSide::Player && participant.isAlive)
            {
                if (combat::CombatParticipant* player = combat->instance->participantBySlot(participant.slot))
                {
                    player->hp = 0;
                    player->isAlive = false;
                }
            }
        }
    }
    else
    {
        finishCombat(*combat);
        return true;
    }

    combat->instance->evaluateOutcome();
    finishCombat(*combat);
    return true;
}

bool CombatManager::killTargetInCombat(const std::string& characterId, uint32_t targetSlot)
{
    ActiveCombat* combat = findCombatForCharacter(characterId);
    if (combat == nullptr)
    {
        return false;
    }

    combat::CombatParticipant* target = combat->instance->participantBySlot(targetSlot);
    if (target == nullptr || !target->isAlive)
    {
        return false;
    }

    target->hp = 0;
    target->isAlive = false;

    net::CombatEventPayload event;
    event.combatId = combat->instance->combatId();
    event.eventType = static_cast<uint8_t>(combat::CombatEventType::Death);
    event.actorSlot = targetSlot;
    event.message = target->name + " was slain by debug command.";
    broadcast_(combat->playerCharacterIds, net::ClientPacketType::CombatEvent, net::serialize(event));

    combat->instance->evaluateOutcome();
    if (combat->instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(*combat);
    }
    else
    {
        broadcastUpdate(*combat);
    }
    return true;
}

void CombatManager::setGodMode(const std::string& characterId, bool enabled)
{
    if (PlayerView* player = findPlayer_(characterId))
    {
        if (player->state != nullptr)
        {
            player->state->godMode = enabled;
            persistState_(characterId, *player->state);
        }
    }
}

} // namespace tbeq::server
