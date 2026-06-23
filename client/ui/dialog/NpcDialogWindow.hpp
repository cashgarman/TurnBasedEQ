#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::ui
{
class GameWindow;
}

namespace tbeq::client
{

class NpcDialogWindow
{
public:
    void applyOpen(const net::NpcDialogOpenPayload& open);
    void clear();
    bool hasDialog() const { return npcEntityId_ != 0; }

    void draw(
        tbeq::ui::GameWindow& window,
        bool& visible,
        int displayWidth,
        int displayHeight);

private:
    uint32_t npcEntityId_ = 0;
    std::string npcName_;
    std::vector<std::string> lines_;
};

} // namespace tbeq::client
