#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq
{

enum class InviteType : uint8_t
{
    Friend,
    Guild,
    Party,
    Trade,
};

enum class InviteState : uint8_t
{
    Pending,
    Accepted,
    Declined,
    Expired,
    Cancelled,
};

struct Invite
{
    std::string inviteId;
    InviteType type = InviteType::Friend;
    InviteState state = InviteState::Pending;
    std::string senderId;
    std::string targetId;
};

bool canTransition(InviteState from, InviteState to);

} // namespace tbeq
