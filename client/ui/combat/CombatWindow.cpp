#include "combat/CombatWindow.hpp"

#include <imgui.h>

#include "ui/GameWindow.hpp"

namespace tbeq::client
{

void CombatWindow::reset()
{
    active_ = false;
    combatId_ = 0;
    currentActorSlot_ = 0;
    turnDurationMs_ = 0;
    playerCombatSlot_ = 0;
    selectedTargetSlot_ = 0;
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
    combatLog_.push_back("Combat started!");
}

void CombatWindow::applyUpdate(const net::CombatUpdatePayload& update)
{
    combatId_ = update.combatId;
    currentActorSlot_ = update.currentActorSlot;
    turnDurationMs_ = update.turnDurationMs;
    participants_ = update.participants;
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

bool CombatWindow::consumePendingAction(net::SubmitActionPayload& outAction)
{
    if (pendingAction_ == PendingAction::None || !active_)
    {
        return false;
    }

    outAction.combatId = combatId_;
    outAction.targetCombatSlot = selectedTargetSlot_;
    switch (pendingAction_)
    {
    case PendingAction::Melee:
        outAction.actionType = 1;
        if (outAction.targetCombatSlot == 0)
        {
            for (const auto& participant : participants_)
            {
                if (participant.side == 1 && participant.isAlive)
                {
                    outAction.targetCombatSlot = participant.combatSlot;
                    break;
                }
            }
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
    default:
        return false;
    }

    pendingAction_ = PendingAction::None;
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

void CombatWindow::draw(tbeq::ui::GameWindow& window, bool& visible, int displayWidth, int displayHeight)
{
    if (!active_)
    {
        visible = false;
        return;
    }

    visible = true;
    window.state().visible = true;
    const bool drawContent = window.begin(displayWidth, displayHeight);
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
        if (isCurrent)
        {
            ImGui::Text("> %s%s (%u/%u)%s",
                participant->name.c_str(),
                isSelf ? " (You)" : "",
                participant->hp,
                participant->maxHp,
                participant->isAlive ? "" : " [DEAD]");
        }
        else
        {
            ImGui::Text("  %s%s (%u/%u)%s",
                participant->name.c_str(),
                isSelf ? " (You)" : "",
                participant->hp,
                participant->maxHp,
                participant->isAlive ? "" : " [DEAD]");
        }
    }

    ImGui::Separator();
    ImGui::Text("Targets");
    for (const auto& participant : participants_)
    {
        if (participant.side != 1 || !participant.isAlive)
        {
            continue;
        }

        const bool selected = selectedTargetSlot_ == participant.combatSlot;
        if (ImGui::Selectable(
                (participant.name + " (" + std::to_string(participant.hp) + "/" + std::to_string(participant.maxHp) + ")").c_str(),
                selected))
        {
            selectedTargetSlot_ = participant.combatSlot;
        }
    }

    ImGui::Separator();
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
}

} // namespace tbeq::client
