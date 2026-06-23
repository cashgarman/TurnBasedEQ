#include "tbeq/npc/DialogTree.hpp"

namespace tbeq
{

const DialogNode* DialogTree::findNode(const std::string& nodeId) const
{
    for (const auto& node : nodes)
    {
        if (node.id == nodeId)
        {
            return &node;
        }
    }
    return nullptr;
}

} // namespace tbeq
