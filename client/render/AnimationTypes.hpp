#pragma once

#include <cstdint>

namespace tbeq::render
{

enum class Facing : uint8_t
{
    South = 0,
    East = 1,
    North = 2,
    West = 3,
};

enum class AnimationClipId : uint8_t
{
    Idle = 0,
    Walk = 1,
};

struct AnimationClip
{
    int frameCount = 1;
    int frameDurationMs = 200;
    bool loop = true;
};

inline AnimationClip clipFor(AnimationClipId id)
{
    switch (id)
    {
    case AnimationClipId::Walk:
        return AnimationClip{4, 125, true};
    case AnimationClipId::Idle:
    default:
        return AnimationClip{4, 200, true};
    }
}

inline Facing facingFromDelta(int32_t dx, int32_t dy)
{
    if (dx > 0)
    {
        return Facing::East;
    }
    if (dx < 0)
    {
        return Facing::West;
    }
    if (dy < 0)
    {
        return Facing::North;
    }
    if (dy > 0)
    {
        return Facing::South;
    }
    return Facing::South;
}

inline int frameIndexForClip(uint64_t tickMs, AnimationClipId clipId, int phaseOffset = 0)
{
    const AnimationClip clip = clipFor(clipId);
    if (clip.frameCount <= 1)
    {
        return 0;
    }
    const int tick = static_cast<int>((tickMs / static_cast<uint64_t>(clip.frameDurationMs) + phaseOffset)
        % static_cast<uint64_t>(clip.frameCount));
    return tick;
}

} // namespace tbeq::render
