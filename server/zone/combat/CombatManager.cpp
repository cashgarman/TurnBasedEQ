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
    const content::SpellCatalog& spellCatalog,
    const content::AbilityCatalog& abilityCatalog,
    const ai::ClassCombatProfileCatalog& aiProfiles,
    FindPlayerFn findPlayer,
    FindAiCompanionsFn findAiCompanions,
    BroadcastFn broadcast,
    PersistStateFn persistState,
    SyncInventoryFn syncInventory)
    : io_(io)
    , skillResolver_(skillResolver)
    , mobCatalog_(mobCatalog)
    , spellCatalog_(spellCatalog)
    , abilityCatalog_(abilityCatalog)
    , classCombatBrain_(aiProfiles, spellCatalog, abilityCatalog)
    , partyMemberAi_(classCombatBrain_)
    , findPlayer_(std::move(findPlayer))
    , findAiCompanions_(std::move(findAiCompanions))
    , broadcast_(std::move(broadcast))
    , persistState_(std::move(persistState))
    , syncInventory_(std::move(syncInventory))
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
    payload.isAiCompanion = participant.isAiCompanion;
    payload.classId = participant.classId;
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
    participant.classId = player.classId;
    participant.weaponSkillId = player.state->equippedWeaponSkill;
    participant.weaponSkillLevel = player.state->skillLevel(player.state->equippedWeaponSkill);
    participant.offenseSkillLevel = player.state->skillLevel("offense");
    participant.defenseSkillLevel = player.state->skillLevel("defense");
    participant.channelingSkillLevel = player.state->skillLevel("channeling");
    participant.unlockedSpells = player.state->unlockedSpells;
    participant.unlockedAbilities = player.state->unlockedAbilities;
    participant.skillLevels.clear();
    for (const auto& [skillId, progress] : player.state->skills)
    {
        participant.skillLevels[skillId] = progress.level;
    }
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

    if (syncInventory_)
    {
        syncInventory_(player.characterId);
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

void CombatManager::addPlayerToCombat(ActiveCombat& combat, PlayerView& player, bool playerControlled, bool aiCompanion)
{
    if (player.state == nullptr)
    {
        return;
    }

    combat.instance->addPlayer(
        player.characterId,
        player.name,
        player.classId,
        player.level,
        *player.state,
        playerControlled,
        aiCompanion);

    for (const auto& participant : combat.instance->participants())
    {
        if (participant.characterId == player.characterId)
        {
            combat.slotToCharacterId[participant.slot] = player.characterId;
            if (player.combatSlot != nullptr)
            {
                *player.combatSlot = participant.slot;
            }
            break;
        }
    }

    if (!aiCompanion)
    {
        if (std::find(combat.playerCharacterIds.begin(), combat.playerCharacterIds.end(), player.characterId)
            == combat.playerCharacterIds.end())
        {
            combat.playerCharacterIds.push_back(player.characterId);
        }
    }

    characterCombatIds_[player.characterId] = combat.instance->combatId();
    if (player.inCombat != nullptr)
    {
        *player.inCombat = true;
    }
    if (player.combatId != nullptr)
    {
        *player.combatId = combat.instance->combatId();
    }
}

void CombatManager::addAiCompanionsToCombat(ActiveCombat& combat, const std::string& leaderCharacterId)
{
    for (PlayerView& aiView : findAiCompanions_(leaderCharacterId))
    {
        addPlayerToCombat(combat, aiView, false, true);
    }
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
    activeCombat.instance = std::make_unique<combat::CombatInstance>(
        combatId,
        skillResolver_,
        mobCatalog_,
        spellCatalog_,
        abilityCatalog_,
        rng_);

    addPlayerToCombat(activeCombat, player, true, false);
    addAiCompanionsToCombat(activeCombat, player.characterId);

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

        if (event.type == combat::CombatEventType::Damage || event.type == combat::CombatEventType::AbilityUsed)
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

void CombatManager::syncAllParticipants(ActiveCombat& combat)
{
    for (const auto& [slot, characterId] : combat.slotToCharacterId)
    {
        if (combat::CombatParticipant* participant = combat.instance->participantBySlot(slot))
        {
            if (PlayerView* player = findPlayer_(characterId))
            {
                syncPlayerFromParticipant(*participant, *player);
                if (player->state != nullptr)
                {
                    persistState_(characterId, *player->state);
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

    if (actor->isAiCompanion)
    {
        resolveAiCompanionTurn(combat);
        return;
    }

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

void CombatManager::resolveAiCompanionTurn(ActiveCombat& combat)
{
    const combat::CombatParticipant* actor = combat.instance->currentActor();
    if (actor == nullptr)
    {
        return;
    }

    ai::CombatBrainSnapshot snapshot;
    snapshot.participants = combat.instance->participants();
    snapshot.actorSlot = actor->slot;

    const combat::CombatActionIntent intent = partyMemberAi_.chooseCombatAction(*actor, snapshot);
    if (!combat.instance->submitAction(intent))
    {
        combat.instance->submitAction(combat::CombatActionIntent{combat::CombatActionType::Defend, 0, {}, {}});
    }

    const auto events = combat.instance->takePendingEvents();
    broadcastEvents(combat, events);
    syncAllParticipants(combat);

    if (combat.instance->outcome() != combat::CombatOutcome::InProgress)
    {
        finishCombat(combat);
        return;
    }

    beginTurn(combat);
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

    combat::CombatActionIntent intent;
    intent.action = action;
    intent.targetSlot = targetSlot;
    if (!combat.instance->submitAction(intent))
    {
        combat.instance->submitAction(combat::CombatActionIntent{combat::CombatActionType::Defend, 0, {}, {}});
    }

    const auto events = combat.instance->takePendingEvents();
    broadcastEvents(combat, events);
    syncAllParticipants(combat);

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

    const auto defaultIntent = combat.instance->defaultPlayerAction(actorSlot);
    combat.instance->submitAction(defaultIntent);

    const auto events = combat.instance->takePendingEvents();
    broadcastEvents(combat, events);
    syncAllParticipants(combat);

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

    combat::CombatActionIntent intent;
    intent.action = static_cast<combat::CombatActionType>(request.actionType);
    intent.targetSlot = request.targetCombatSlot;
    intent.spellId = request.spellId;
    intent.abilityId = request.abilityId;

    const combat::CombatParticipant* expected = combat->instance->currentActor();
    if (expected == nullptr || expected->slot != slotIt->first)
    {
        result.message = "Not your turn";
        return false;
    }

    if (!combat->instance->submitAction(intent))
    {
        result.message = "Invalid action";
        return false;
    }

    const auto events = combat->instance->takePendingEvents();
    broadcastEvents(*combat, events);
    syncAllParticipants(*combat);

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

    for (const auto& [slot, characterId] : combat.slotToCharacterId)
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
                if (combat::CombatParticipant* playerParticipant = combat->instance->participantBySlot(participant.slot))
                {
                    playerParticipant->hp = 0;
                    playerParticipant->isAlive = false;
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

void CombatManager::fillMana(const std::string& characterId)
{
    if (PlayerView* player = findPlayer_(characterId))
    {
        if (player->state != nullptr)
        {
            player->state->mana = player->state->maxMana;
            persistState_(characterId, *player->state);

            net::CharacterVitalsPayload vitals;
            vitals.hp = player->state->hp;
            vitals.maxHp = player->state->maxHp;
            vitals.mana = player->state->mana;
            vitals.maxMana = player->state->maxMana;
            broadcast_({characterId}, net::ClientPacketType::CharacterVitals, net::serialize(vitals));
        }
    }
}

void CombatManager::unlockAllSpells(const std::string& characterId)
{
    if (PlayerView* player = findPlayer_(characterId))
    {
        if (player->state == nullptr)
        {
            return;
        }

        std::vector<std::string> spellIds;
        for (const auto* spell : spellCatalog_.spellsForClass(player->classId, player->level))
        {
            spellIds.push_back(spell->id);
        }
        std::vector<std::string> abilityIds;
        for (const auto* ability : abilityCatalog_.abilitiesForClass(player->classId, player->level))
        {
            abilityIds.push_back(ability->id);
        }
        player->state->unlockAllClassContent(spellIds, abilityIds);
        persistState_(characterId, *player->state);
    }
}

} // namespace tbeq::server
