#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::ui
{
class GameWindow;
}

namespace tbeq::client
{

class CombatWindow
{
public:
    void reset();
    void applyStart(const net::CombatStartPayload& start);
    void applyUpdate(const net::CombatUpdatePayload& update);
    void applyEvent(const net::CombatEventPayload& event);
    void applyEnd(const net::CombatEndPayload& end);
    void applyVitals(const net::CharacterVitalsPayload& vitals);
    void applySkillGain(const net::SkillGainPayload& gain);

    bool isActive() const { return active_; }
    uint32_t combatId() const { return combatId_; }
    uint32_t playerCombatSlot() const { return playerCombatSlot_; }
    uint32_t selectedTargetSlot() const { return selectedTargetSlot_; }

    void setPlayerCombatSlot(uint32_t slot) { playerCombatSlot_ = slot; }
    void queueMelee();
    void queueDefend();
    void queueFlee();
    bool consumePendingAction(net::SubmitActionPayload& outAction);
    void draw(tbeq::ui::GameWindow& window, bool& visible, int displayWidth, int displayHeight);

private:
    enum class PendingAction
    {
        None = 0,
        Melee = 1,
        Defend = 2,
        Flee = 3,
    };

    bool active_ = false;
    uint32_t combatId_ = 0;
    uint32_t currentActorSlot_ = 0;
    uint32_t turnDurationMs_ = 0;
    uint32_t playerCombatSlot_ = 0;
    uint32_t selectedTargetSlot_ = 0;
    uint16_t playerHp_ = 0;
    uint16_t playerMaxHp_ = 0;
    uint16_t playerMana_ = 0;
    uint16_t playerMaxMana_ = 0;
    std::vector<net::CombatParticipantPayload> participants_;
    std::vector<uint32_t> turnOrder_;
    std::deque<std::string> combatLog_;
    PendingAction pendingAction_ = PendingAction::None;
};

} // namespace tbeq::client
