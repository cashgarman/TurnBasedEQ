#pragma once

#include <string>
#include <vector>

#include "tbeq/ai/ClassCombatProfile.hpp"
#include "tbeq/combat/CombatTypes.hpp"
#include "tbeq/content/AbilityCatalog.hpp"
#include "tbeq/content/SpellCatalog.hpp"

namespace tbeq::ai
{

struct CombatBrainSnapshot
{
    std::vector<combat::CombatParticipant> participants;
    uint32_t actorSlot = 0;
};

class ClassCombatBrain
{
public:
    ClassCombatBrain(
        const ClassCombatProfileCatalog& profiles,
        const content::SpellCatalog& spells,
        const content::AbilityCatalog& abilities);

    combat::CombatActionIntent chooseAction(
        const combat::CombatParticipant& actor,
        const CombatBrainSnapshot& snapshot) const;

    bool shouldHealAlly(float allyHpPercent, const std::string& classId) const;

private:
    const ClassCombatProfileCatalog& profiles_;
    const content::SpellCatalog& spells_;
    const content::AbilityCatalog& abilities_;

    uint32_t findLowestHpAllySlot(
        const combat::CombatParticipant& actor,
        const CombatBrainSnapshot& snapshot) const;
    uint32_t findEnemyTargetSlot(const combat::CombatParticipant& actor, const CombatBrainSnapshot& snapshot) const;
};

} // namespace tbeq::ai
