#pragma once

#include <string>

namespace tbeq
{

enum class ChatChannel
{
    Say,
    Shout,
    Ooc,
    Tell,
    Party,
    Guild,
    System,
};

std::string chatChannelName(ChatChannel channel);
bool isWorldLoginRouted(ChatChannel channel);

} // namespace tbeq
