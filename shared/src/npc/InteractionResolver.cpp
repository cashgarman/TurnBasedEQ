#include "tbeq/npc/InteractionResolver.hpp"

namespace tbeq
{

InteractionResult InteractionResolver::resolve(const InteractionRequest& request, const DialogTree& tree) const
{
    InteractionResult result;
    if (tree.nodes.empty())
    {
        result.message = "No dialog available for NPC " + request.npcId;
        return result;
    }

    result.ok = true;
    result.page = &tree.nodes.front();
    result.message = "Showing dialog root";
    return result;
}

} // namespace tbeq
