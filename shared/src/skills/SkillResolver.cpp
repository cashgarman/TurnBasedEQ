#include "tbeq/skills/SkillResolver.hpp"

namespace tbeq
{

uint16_t SkillResolver::getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const
{
    (void)classId;
    (void)skillId;
    return static_cast<uint16_t>(5 * characterLevel);
}

bool SkillResolver::canUseSkill(const SkillProgress& progress, uint16_t cap) const
{
    return progress.level <= cap;
}

void SkillResolver::applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap)
{
    if (progress.level >= cap)
    {
        return;
    }
    progress.experience += amount;
    while (progress.experience >= 100 && progress.level < cap)
    {
        progress.experience -= 100;
        ++progress.level;
    }
}

} // namespace tbeq
