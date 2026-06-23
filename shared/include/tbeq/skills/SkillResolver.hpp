#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "tbeq/skills/SkillDef.hpp"

namespace tbeq
{

struct SkillProgress
{
    uint16_t level = 1;
    uint32_t experience = 0;
};

class SkillResolver
{
public:
    uint16_t getCap(const std::string& classId, const std::string& skillId, uint16_t characterLevel) const;
    bool canUseSkill(const SkillProgress& progress, uint16_t cap) const;
    void applyGain(SkillProgress& progress, uint32_t amount, uint16_t cap);

private:
    std::unordered_map<std::string, uint16_t> defaultCaps_;
};

} // namespace tbeq
