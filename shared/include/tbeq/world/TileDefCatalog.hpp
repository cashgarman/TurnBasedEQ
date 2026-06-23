#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace tbeq::world
{

enum class TileCollision
{
    Walkable,
    Blocked,
    Water,
};

struct TileDef
{
    std::string id;
    std::string category;
    std::string autotileGroup;
    std::string animation;
    TileCollision collision = TileCollision::Walkable;
};

class TileDefCatalog
{
public:
    bool loadFromFile(const std::filesystem::path& path);

    const TileDef* find(const std::string& tileId) const;
    bool isWalkable(const std::string& tileId) const;
    TileCollision collision(const std::string& tileId) const;
    std::string autotileGroup(const std::string& tileId) const;

    const std::unordered_map<std::string, TileDef>& defs() const { return defs_; }

private:
    static TileCollision parseCollision(const std::string& value);

    std::unordered_map<std::string, TileDef> defs_;
};

} // namespace tbeq::world
