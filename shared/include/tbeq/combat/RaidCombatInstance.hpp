#pragma once

#include "tbeq/combat/CombatInstance.hpp"

namespace tbeq::combat
{

// Phase 10: extended combat instance for 12+ player raids with multi-phase boss scripts.
class RaidCombatInstance : public CombatInstance
{
public:
    using CombatInstance::CombatInstance;

    static constexpr size_t kMaxRaidParticipants = 24;
};

} // namespace tbeq::combat
