#include "tbeq/social/Invite.hpp"

namespace tbeq
{

bool canTransition(InviteState from, InviteState to)
{
    if (from != InviteState::Pending)
    {
        return false;
    }
    return to == InviteState::Accepted
        || to == InviteState::Declined
        || to == InviteState::Expired
        || to == InviteState::Cancelled;
}

} // namespace tbeq
