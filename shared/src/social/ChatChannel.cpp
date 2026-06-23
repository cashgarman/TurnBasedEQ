#include "tbeq/social/ChatChannel.hpp"

namespace tbeq
{

std::string chatChannelName(ChatChannel channel)
{
    switch (channel)
    {
    case ChatChannel::Say: return "Say";
    case ChatChannel::Shout: return "Shout";
    case ChatChannel::Ooc: return "OOC";
    case ChatChannel::Tell: return "Tell";
    case ChatChannel::Party: return "Party";
    case ChatChannel::Guild: return "Guild";
    case ChatChannel::System: return "System";
    default: return "Unknown";
    }
}

bool isWorldLoginRouted(ChatChannel channel)
{
    return channel == ChatChannel::Tell || channel == ChatChannel::Guild;
}

} // namespace tbeq
