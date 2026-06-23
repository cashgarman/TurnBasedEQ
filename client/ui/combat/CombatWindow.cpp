#include "combat/CombatWindow.hpp"

#include <imgui.h>

#include "ui/GameWindow.hpp"

namespace tbeq::client
{

namespace
{

uint32_t defaultEnemyTarget(const std::vector<net::CombatParticipantPayload>& participants)
{
    for (const auto& participant : participants)
    {
        if (participant.side == 1 && participant.isAlive)
        {
            return participant.combatSlot;
        }
    }
    return 0;
}

uint32_t defaultAllyTarget(
    const std::vector<net::CombatParticipantPayload>& participants,
    uint32_t playerSlot)
{
    for (const auto& participant : participants)
    {
        if (participant.side == 0 && participant.isAlive && participant.combatSlot != playerSlot)
        {
            return participant.combatSlot;
        }
    }
    return playerSlot;
}

} // namespace

void CombatWindow::setSpriteRenderer(SpriteRenderer* sprites)
{
    sprites_ = sprites;
}

void CombatWindow::updateAnimation(uint64_t tickMs)
{
    animTickMs_ = tickMs;
}

void CombatWindow::reset()
{
    active_ = false;
    combatId_ = 0;
    currentActorSlot_ = 0;
    turnDurationMs_ = 0;
    playerCombatSlot_ = 0;
    selectedTargetSlot_ = 0;
    playerClassId_.clear();
    pendingSpellId_.clear();
    pendingAbilityId_.clear();
    participants_.clear();
    turnOrder_.clear();
    combatLog_.clear();
}

void CombatWindow::applyStart(const net::CombatStartPayload& start)
{
    active_ = true;
    combatId_ = start.combatId;
    currentActorSlot_ = start.currentActorSlot;
    turnDurationMs_ = start.turnDurationMs;
    participants_ = start.participants;
    turnOrder_ = start.turnOrder;
    for (const auto& participant : start.participants)
    {
        if (participant.isPlayerControlled)
        {
            playerClassId_ = participant.classId;
            playerMana_ = participant.mana;
            playerMaxMana_ = participant.maxMana;
            playerHp_ = participant.hp;
            playerMaxHp_ = participant.maxHp;
        }
    }
    combatLog_.push_back("Combat started!");
}

void CombatWindow::applyUpdate(const net::CombatUpdatePayload& update)
{
    combatId_ = update.combatId;
    currentActorSlot_ = update.currentActorSlot;
    turnDurationMs_ = update.turnDurationMs;
    participants_ = update.participants;
    for (const auto& participant : update.participants)
    {
        if (participant.combatSlot == playerCombatSlot_)
        {
            playerMana_ = participant.mana;
            playerMaxMana_ = participant.maxMana;
            playerHp_ = participant.hp;
            playerMaxHp_ = participant.maxHp;
        }
    }
}

void CombatWindow::applyEvent(const net::CombatEventPayload& event)
{
    if (!event.message.empty())
    {
        combatLog_.push_back(event.message);
        while (combatLog_.size() > 100)
        {
            combatLog_.pop_front();
        }
    }

    if (event.actorSlot != 0 && (event.eventType == 1 || event.eventType == 2))
    {
        attackFlashSlot_ = event.actorSlot;
        attackFlashUntilMs_ = animTickMs_ + 250;
    }
}

void CombatWindow::queueMelee()
{
    pendingAction_ = PendingAction::Melee;
}

void CombatWindow::queueDefend()
{
    pendingAction_ = PendingAction::Defend;
}

void CombatWindow::queueFlee()
{
    pendingAction_ = PendingAction::Flee;
}

void CombatWindow::queueSpell(const std::string& spellId)
{
    pendingAction_ = PendingAction::Spell;
    pendingSpellId_ = spellId;
}

void CombatWindow::queueAbility(const std::string& abilityId)
{
    pendingAction_ = PendingAction::Ability;
    pendingAbilityId_ = abilityId;
}

bool CombatWindow::consumePendingAction(net::SubmitActionPayload& outAction)
{
    if (pendingAction_ == PendingAction::None || !active_)
    {
        return false;
    }

    outAction.combatId = combatId_;
    outAction.targetCombatSlot = selectedTargetSlot_;
    outAction.spellId.clear();
    outAction.abilityId.clear();

    switch (pendingAction_)
    {
    case PendingAction::Melee:
        outAction.actionType = 1;
        if (outAction.targetCombatSlot == 0)
        {
            outAction.targetCombatSlot = defaultEnemyTarget(participants_);
        }
        break;
    case PendingAction::Defend:
        outAction.actionType = 2;
        outAction.targetCombatSlot = 0;
        break;
    case PendingAction::Flee:
        outAction.actionType = 3;
        outAction.targetCombatSlot = 0;
        break;
    case PendingAction::Spell:
        outAction.actionType = 4;
        outAction.spellId = pendingSpellId_;
        if (pendingSpellId_ == "cleric_heal")
        {
            if (outAction.targetCombatSlot == 0)
            {
                outAction.targetCombatSlot = defaultAllyTarget(participants_, playerCombatSlot_);
            }
        }
        else if (outAction.targetCombatSlot == 0)
        {
            outAction.targetCombatSlot = defaultEnemyTarget(participants_);
        }
        break;
    case PendingAction::Ability:
        outAction.actionType = 5;
        outAction.abilityId = pendingAbilityId_;
        if (outAction.targetCombatSlot == 0)
        {
            outAction.targetCombatSlot = defaultEnemyTarget(participants_);
        }
        break;
    default:
        return false;
    }

    pendingAction_ = PendingAction::None;
    pendingSpellId_.clear();
    pendingAbilityId_.clear();
    return true;
}

void CombatWindow::applyEnd(const net::CombatEndPayload& end)
{
    combatLog_.push_back(end.message);
    active_ = false;
}

void CombatWindow::applyVitals(const net::CharacterVitalsPayload& vitals)
{
    playerHp_ = vitals.hp;
    playerMaxHp_ = vitals.maxHp;
    playerMana_ = vitals.mana;
    playerMaxMana_ = vitals.maxMana;
}

void CombatWindow::applySkillGain(const net::SkillGainPayload& gain)
{
    combatLog_.push_back(gain.message);
}

EntitySpriteRequest CombatWindow::spriteRequestForParticipant(
    const net::CombatParticipantPayload& participant) const
{
    EntitySpriteRequest request;
    if (!participant.mobId.empty())
    {
        request.entityType = 2;
        request.appearanceId = participant.mobId;
    }
    else
    {
        request.entityType = 0;
        request.raceId = participant.raceId.empty() ? "human" : participant.raceId;
        request.classId = participant.classId;
    }

    const bool attackFlash = participant.combatSlot == attackFlashSlot_
        && animTickMs_ < attackFlashUntilMs_;
    request.clipId = attackFlash ? render::AnimationClipId::Walk : render::AnimationClipId::Idle;
    request.facing = participant.side == 1 ? render::Facing::West : render::Facing::East;
    request.frameIndex = render::frameIndexForClip(
        animTickMs_,
        request.clipId,
        static_cast<int>(participant.combatSlot % 4));
    return request;
}

void CombatWindow::drawParticipantSprite(
    const net::CombatParticipantPayload& participant,
    bool isCurrentActor)
{
    if (sprites_ == nullptr)
    {
        return;
    }

    SDL_Texture* texture = sprites_->textureForEntity(spriteRequestForParticipant(participant));
    if (texture == nullptr)
    {
        return;
    }

    if (isCurrentActor)
    {
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 220, 80, 255));
    }

    ImGui::Image(
        reinterpret_cast<ImTextureID>(texture),
        ImVec2(48.0f, 48.0f),
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f));

    if (isCurrentActor)
    {
        ImGui::PopStyleColor();
    }
}

void CombatWindow::draw(tbeq::ui::GameWindow& window, bool& visible, int displayWidth, int displayHeight)
{
    if (!active_)
    {
        visible = false;
        window.state().visible = false;
        return;
    }

    if (!visible)
    {
        window.state().visible = false;
        return;
    }

    const bool drawContent = window.begin(displayWidth, displayHeight);
    visible = window.state().visible;
    if (!visible)
    {
        window.end();
        return;
    }
    if (!drawContent)
    {
        window.end();
        return;
    }

    ImGui::Text("Combat #%u", combatId_);
    ImGui::ProgressBar(
        playerMaxHp_ > 0 ? static_cast<float>(playerHp_) / static_cast<float>(playerMaxHp_) : 0.0f,
        ImVec2(-1.0f, 0.0f),
        (std::to_string(playerHp_) + " / " + std::to_string(playerMaxHp_)).c_str());
    ImGui::Text("Mana: %u / %u", playerMana_, playerMaxMana_);
    ImGui::Separator();

    ImGui::Text("Combatants");
    if (ImGui::BeginTable("CombatSprites", static_cast<int>(turnOrder_.size()), ImGuiTableFlags_SizingFixedFit))
    {
        for (uint32_t slot : turnOrder_)
        {
            ImGui::TableSetupColumn("sprite", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        }
        ImGui::TableNextRow();
        for (uint32_t slot : turnOrder_)
        {
            const net::CombatParticipantPayload* participant = nullptr;
            for (const auto& entry : participants_)
            {
                if (entry.combatSlot == slot)
                {
                    participant = &entry;
                    break;
                }
            }
            ImGui::TableNextColumn();
            if (participant != nullptr && participant->isAlive)
            {
                drawParticipantSprite(*participant, slot == currentActorSlot_);
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Turn order");
    for (uint32_t slot : turnOrder_)
    {
        const net::CombatParticipantPayload* participant = nullptr;
        for (const auto& entry : participants_)
        {
            if (entry.combatSlot == slot)
            {
                participant = &entry;
                break;
            }
        }
        if (participant == nullptr)
        {
            continue;
        }

        const bool isCurrent = slot == currentActorSlot_;
        const bool isSelf = slot == playerCombatSlot_;
        const char* aiTag = participant->isAiCompanion ? " [AI]" : "";
        if (isCurrent)
        {
            ImGui::Text("> %s%s (%u/%u)%s%s",
                participant->name.c_str(),
                isSelf ? " (You)" : "",
                participant->hp,
                participant->maxHp,
                participant->isAlive ? "" : " [DEAD]",
                aiTag);
        }
        else
        {
            ImGui::Text("  %s%s (%u/%u)%s%s",
                participant->name.c_str(),
                isSelf ? " (You)" : "",
                participant->hp,
                participant->maxHp,
                participant->isAlive ? "" : " [DEAD]",
                aiTag);
        }
    }

    ImGui::Separator();
    ImGui::Text("Targets");
    for (const auto& participant : participants_)
    {
        if (!participant.isAlive)
        {
            continue;
        }

        const bool selected = selectedTargetSlot_ == participant.combatSlot;
        const char* sideLabel = participant.side == 1 ? "Enemy" : "Ally";
        const std::string label = participant.name + " [" + sideLabel + "] ("
            + std::to_string(participant.hp) + "/" + std::to_string(participant.maxHp) + ")";
        if (ImGui::Selectable(label.c_str(), selected))
        {
            selectedTargetSlot_ = participant.combatSlot;
        }
    }

    ImGui::Separator();
    ImGui::Text("Ability bar");
    const bool isPlayerTurn = active_ && currentActorSlot_ == playerCombatSlot_;
    if (!isPlayerTurn)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Melee Attack") && isPlayerTurn)
    {
        queueMelee();
    }
    ImGui::SameLine();
    if (ImGui::Button("Defend") && isPlayerTurn)
    {
        queueDefend();
    }
    ImGui::SameLine();
    if (ImGui::Button("Flee") && isPlayerTurn)
    {
        queueFlee();
    }

    if (playerClassId_ == "cleric")
    {
        if (ImGui::Button("Minor Heal (10 mana)") && isPlayerTurn)
        {
            queueSpell("cleric_heal");
        }
    }
    else if (playerClassId_ == "wizard")
    {
        if (ImGui::Button("Shock Bolt (15 mana)") && isPlayerTurn)
        {
            queueSpell("wizard_nuke");
        }
    }
    else if (playerClassId_ == "warrior")
    {
        if (ImGui::Button("Bash") && isPlayerTurn)
        {
            queueAbility("warrior_bash");
        }
        ImGui::SameLine();
        if (ImGui::Button("Kick") && isPlayerTurn)
        {
            queueAbility("warrior_kick");
        }
    }
    else if (playerClassId_ == "rogue")
    {
        if (ImGui::Button("Backstab") && isPlayerTurn)
        {
            queueAbility("rogue_backstab");
        }
    }

    if (!isPlayerTurn)
    {
        ImGui::EndDisabled();
        ImGui::TextUnformatted("Waiting for turn...");
    }

    ImGui::Separator();
    ImGui::BeginChild("CombatLog", ImVec2(0.0f, 120.0f), true);
    for (const auto& line : combatLog_)
    {
        ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::EndChild();

    window.end();
    visible = window.state().visible;
}

} // namespace tbeq::client
