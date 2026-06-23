#pragma once

#include <optional>
#include <string>
#include <vector>

namespace tbeq
{

struct DialogResponse
{
    std::string label;
    std::optional<std::string> nextNodeId;
};

struct DialogNode
{
    std::string id;
    std::string text;
    std::vector<DialogResponse> responses;
};

struct DialogTree
{
    std::string id;
    std::vector<DialogNode> nodes;

    const DialogNode* findNode(const std::string& nodeId) const;
};

} // namespace tbeq
