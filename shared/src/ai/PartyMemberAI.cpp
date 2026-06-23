#include "tbeq/ai/PartyMemberAI.hpp"

namespace tbeq::ai
{

PartyMemberAI::PartyMemberAI(ClassCombatBrain& brain)
    : brain_(brain)
{
}

combat::CombatActionIntent PartyMemberAI::chooseCombatAction(
    const combat::CombatParticipant& actor,
    const CombatBrainSnapshot& snapshot) const
{
    return brain_.chooseAction(actor, snapshot);
}

} // namespace tbeq::ai
