#include "tbeq/world/TileDefCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::world
{

TileCollision TileDefCatalog::parseCollision(const std::string& value)
{
    if (value == "blocked")
    {
        return TileCollision::Blocked;
    }
    if (value == "water")
    {
        return TileCollision::Water;
    }
    return TileCollision::Walkable;
}

bool TileDefCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;
    if (!json.is_array())
    {
        return false;
    }

    defs_.clear();
    for (const auto& entry : json)
    {
        if (!entry.contains("id"))
        {
            continue;
        }

        TileDef def;
        def.id = entry["id"].get<std::string>();
        def.category = entry.value("category", def.id);
        def.autotileGroup = entry.value("autotileGroup", def.category);
        def.animation = entry.value("animation", "none");
        def.collision = parseCollision(entry.value("collision", "walkable"));
        defs_[def.id] = std::move(def);
    }

    return !defs_.empty();
}

const TileDef* TileDefCatalog::find(const std::string& tileId) const
{
    const auto it = defs_.find(tileId);
    if (it == defs_.end())
    {
        return nullptr;
    }
    return &it->second;
}

bool TileDefCatalog::isWalkable(const std::string& tileId) const
{
    const auto* def = find(tileId);
    if (def == nullptr)
    {
        return false;
    }
    return def->collision != TileCollision::Blocked;
}

TileCollision TileDefCatalog::collision(const std::string& tileId) const
{
    const auto* def = find(tileId);
    if (def == nullptr)
    {
        return TileCollision::Blocked;
    }
    return def->collision;
}

std::string TileDefCatalog::autotileGroup(const std::string& tileId) const
{
    const auto* def = find(tileId);
    if (def == nullptr)
    {
        return tileId;
    }
    return def->autotileGroup;
}

} // namespace tbeq::world
