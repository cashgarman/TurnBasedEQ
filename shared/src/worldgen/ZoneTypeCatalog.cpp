#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tbeq::worldgen
{

bool ZoneTypeCatalog::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    input >> json;

    if (json.contains("city"))
    {
        CityTemplate city;
        const auto& node = json["city"];
        city.width = node.value("width", 64);
        city.height = node.value("height", 64);
        city.merchantSlots = node.value("merchantSlots", 2);
        city.lorekeeperSlots = node.value("lorekeeperSlots", 3);
        city_ = city;
        defaults_["city"] = ZoneTypeDefaults{"temperate", "starter_city", true};
    }

    if (json.contains("hunting"))
    {
        HuntingTemplate hunting;
        const auto& node = json["hunting"];
        hunting.width = node.value("width", 128);
        hunting.height = node.value("height", 128);
        hunting.spawnCampCount = node.value("spawnCampCount", 4);
        hunting_ = hunting;
        defaults_["hunting"] = ZoneTypeDefaults{"forest", "hunting_grounds", false};
    }

    if (json.contains("dungeon"))
    {
        DungeonTemplate dungeon;
        const auto& node = json["dungeon"];
        dungeon.width = node.value("width", 48);
        dungeon.height = node.value("height", 48);
        dungeon.roomCount = node.value("roomCount", 12);
        dungeon.bossRoomDepth = node.value("bossRoomDepth", 8);
        dungeon_ = dungeon;
        defaults_["dungeon"] = ZoneTypeDefaults{"cavern", "hunting_grounds", false};
    }

    return city_.has_value() && hunting_.has_value() && dungeon_.has_value();
}

std::optional<CityTemplate> ZoneTypeCatalog::cityTemplate() const
{
    return city_;
}

std::optional<HuntingTemplate> ZoneTypeCatalog::huntingTemplate() const
{
    return hunting_;
}

std::optional<DungeonTemplate> ZoneTypeCatalog::dungeonTemplate() const
{
    return dungeon_;
}

std::optional<ZoneTypeDefaults> ZoneTypeCatalog::defaultsForType(const std::string& zoneType) const
{
    const auto it = defaults_.find(zoneType);
    if (it == defaults_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

} // namespace tbeq::worldgen
