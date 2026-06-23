#include "tbeq/ai/ClassCombatBrain.hpp"

#include <algorithm>
#include <cmath>

namespace tbeq::ai
{

ClassCombatBrain::ClassCombatBrain(
    const ClassCombatProfileCatalog& profiles,
    const content::SpellCatalog& spells,
    const content::AbilityCatalog& abilities)
    : profiles_(profiles)
    , spells_(spells)
    , abilities_(abilities)
{
}

bool ClassCombatBrain::shouldHealAlly(float allyHpPercent, const std::string& classId) const
{
    const ClassCombatProfile* profile = profiles_.findProfile(classId);
    if (profile == nullptr || profile->preferredHealSpell.empty())
    {
        return false;
    }
    return allyHpPercent <= profile->healThreshold;
}

uint32_t ClassCombatBrain::findLowestHpAllySlot(
    const combat::CombatParticipant& actor,
    const CombatBrainSnapshot& snapshot) const
{
    const combat::CombatParticipant* best = nullptr;
    float bestRatio = 2.0f;

    for (const auto& participant : snapshot.participants)
    {
        if (!participant.isAlive || participant.side != actor.side || participant.maxHp == 0)
        {
            continue;
        }
        const float ratio = static_cast<float>(participant.hp) / static_cast<float>(participant.maxHp);
        if (ratio < bestRatio)
        {
            bestRatio = ratio;
            best = &participant;
        }
    }

    return best != nullptr ? best->slot : 0;
}

uint32_t ClassCombatBrain::findEnemyTargetSlot(
    const combat::CombatParticipant& actor,
    const CombatBrainSnapshot& snapshot) const
{
    const combat::CombatParticipant* best = nullptr;
    for (const auto& participant : snapshot.participants)
    {
        if (!participant.isAlive || participant.side == actor.side)
        {
            continue;
        }
        if (best == nullptr || participant.hp < best->hp)
        {
            best = &participant;
        }
    }
    return best != nullptr ? best->slot : 0;
}

combat::CombatActionIntent ClassCombatBrain::chooseAction(
    const combat::CombatParticipant& actor,
    const CombatBrainSnapshot& snapshot) const
{
    combat::CombatActionIntent intent;
    const ClassCombatProfile* profile = profiles_.findProfile(actor.classId);

    if (profile != nullptr)
    {
        const float selfHpRatio = actor.maxHp > 0
            ? static_cast<float>(actor.hp) / static_cast<float>(actor.maxHp)
            : 1.0f;
        if (selfHpRatio <= profile->fleeHpPercent)
        {
            intent.action = combat::CombatActionType::Flee;
            return intent;
        }
    }

    if (actor.classId == "cleric" && profile != nullptr && !profile->preferredHealSpell.empty())
    {
        const uint32_t allySlot = findLowestHpAllySlot(actor, snapshot);
        if (allySlot != 0)
        {
            for (const auto& participant : snapshot.participants)
            {
                if (participant.slot != allySlot)
                {
                    continue;
                }
                const float ratio = participant.maxHp > 0
                    ? static_cast<float>(participant.hp) / static_cast<float>(participant.maxHp)
                    : 1.0f;
                if (shouldHealAlly(ratio, actor.classId))
                {
                    const content::SpellDef* spell = spells_.findSpell(profile->preferredHealSpell);
                    if (spell != nullptr && actor.mana >= spell->manaCost)
                    {
                        intent.action = combat::CombatActionType::CastSpell;
                        intent.spellId = spell->id;
                        intent.targetSlot = allySlot;
                        return intent;
                    }
                }
                break;
            }
        }
    }

    if (actor.classId == "wizard" && profile != nullptr && !profile->preferredNukeSpell.empty())
    {
        const content::SpellDef* spell = spells_.findSpell(profile->preferredNukeSpell);
        const uint32_t enemySlot = findEnemyTargetSlot(actor, snapshot);
        if (spell != nullptr && enemySlot != 0 && actor.mana >= spell->manaCost)
        {
            intent.action = combat::CombatActionType::CastSpell;
            intent.spellId = spell->id;
            intent.targetSlot = enemySlot;
            return intent;
        }
    }

    if (profile != nullptr && !profile->preferredAbility.empty())
    {
        const content::AbilityDef* ability = abilities_.findAbility(profile->preferredAbility);
        const uint32_t enemySlot = findEnemyTargetSlot(actor, snapshot);
        if (ability != nullptr && enemySlot != 0)
        {
            intent.action = combat::CombatActionType::UseAbility;
            intent.abilityId = ability->id;
            intent.targetSlot = enemySlot;
            return intent;
        }
    }

    const uint32_t enemySlot = findEnemyTargetSlot(actor, snapshot);
    if (enemySlot != 0)
    {
        intent.action = combat::CombatActionType::MeleeAttack;
        intent.targetSlot = enemySlot;
        return intent;
    }

    intent.action = combat::CombatActionType::Defend;
    return intent;
}

} // namespace tbeq::ai
