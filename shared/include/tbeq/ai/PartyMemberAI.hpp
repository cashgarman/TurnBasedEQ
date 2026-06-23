#pragma once

#include "tbeq/ai/ClassCombatBrain.hpp"

namespace tbeq::ai
{

class PartyMemberAI
{
public:
    explicit PartyMemberAI(ClassCombatBrain& brain);

    combat::CombatActionIntent chooseCombatAction(
        const combat::CombatParticipant& actor,
        const CombatBrainSnapshot& snapshot) const;

private:
    ClassCombatBrain& brain_;
};

} // namespace tbeq::ai
