#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <SDL.h>

#include "render/AnimationTypes.hpp"
#include "render/SpriteRenderer.hpp"
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
    void setSpriteRenderer(SpriteRenderer* sprites);

    void reset();
    void applyStart(const net::CombatStartPayload& start);
    void applyUpdate(const net::CombatUpdatePayload& update);
    void applyEvent(const net::CombatEventPayload& event);
    void applyEnd(const net::CombatEndPayload& end);
    void applyVitals(const net::CharacterVitalsPayload& vitals);
    void applySkillGain(const net::SkillGainPayload& gain);

    void updateAnimation(uint64_t tickMs);

    bool isActive() const { return active_; }
    uint32_t combatId() const { return combatId_; }
    uint32_t playerCombatSlot() const { return playerCombatSlot_; }
    uint32_t selectedTargetSlot() const { return selectedTargetSlot_; }
    const std::string& playerClassId() const { return playerClassId_; }

    void setPlayerCombatSlot(uint32_t slot) { playerCombatSlot_ = slot; }
    void queueMelee();
    void queueDefend();
    void queueFlee();
    void queueSpell(const std::string& spellId);
    void queueAbility(const std::string& abilityId);
    bool consumePendingAction(net::SubmitActionPayload& outAction);
    void draw(tbeq::ui::GameWindow& window, bool& visible, int displayWidth, int displayHeight);

private:
    enum class PendingAction
    {
        None = 0,
        Melee = 1,
        Defend = 2,
        Flee = 3,
        Spell = 4,
        Ability = 5,
    };

    EntitySpriteRequest spriteRequestForParticipant(const net::CombatParticipantPayload& participant) const;
    void drawParticipantSprite(const net::CombatParticipantPayload& participant, bool isCurrentActor);

    SpriteRenderer* sprites_ = nullptr;
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
    std::string playerClassId_;
    std::string pendingSpellId_;
    std::string pendingAbilityId_;
    std::vector<net::CombatParticipantPayload> participants_;
    std::vector<uint32_t> turnOrder_;
    std::deque<std::string> combatLog_;
    PendingAction pendingAction_ = PendingAction::None;
    uint64_t animTickMs_ = 0;
    uint32_t attackFlashSlot_ = 0;
    uint64_t attackFlashUntilMs_ = 0;
};

} // namespace tbeq::client
