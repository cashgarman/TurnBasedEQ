#pragma once

#include <string>

#include "tbeq/npc/DialogTree.hpp"

namespace tbeq
{

enum class NpcRole
{
    Lorekeeper,
    QuestGiver,
    Merchant,
    Trainer,
    Combat,
};

struct InteractionRequest
{
    std::string npcId;
    std::string characterId;
};

struct InteractionResult
{
    bool ok = false;
    std::string message;
    const DialogNode* page = nullptr;
};

class InteractionResolver
{
public:
    InteractionResult resolve(const InteractionRequest& request, const DialogTree& tree) const;
};

} // namespace tbeq
