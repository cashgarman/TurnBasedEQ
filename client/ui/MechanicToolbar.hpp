#pragma once

#include <functional>
#include <vector>

namespace tbeq::client
{

struct MechanicToolbarButton
{
    const char* label = "";
    const char* shortcut = nullptr;
    bool enabled = true;
    bool active = false;
    std::function<void()> onPress;
};

class MechanicToolbar
{
public:
    static constexpr float kHeight = 36.0f;

    void draw(int screenWidth, const std::vector<MechanicToolbarButton>& buttons);
};

} // namespace tbeq::client
