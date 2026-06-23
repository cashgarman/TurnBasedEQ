#include "tbeq/worldgen/WorldLayoutGraph.hpp"

#include "tbeq/worldgen/ZoneTypeCatalog.hpp"

namespace tbeq::worldgen
{

GeneratedWorld buildStarterWorldGraph(const ZoneTypeCatalog& catalog)
{
    GeneratedWorld world;
    world.version = "0.1.0";

    const auto cityTemplate = catalog.cityTemplate();
    const auto huntingTemplate = catalog.huntingTemplate();
    const auto dungeonTemplate = catalog.dungeonTemplate();
    if (!cityTemplate || !huntingTemplate || !dungeonTemplate)
    {
        return world;
    }

    const auto cityDefaults = catalog.defaultsForType("city");
    const auto huntingDefaults = catalog.defaultsForType("hunting");
    const auto dungeonDefaults = catalog.defaultsForType("dungeon");
    if (!cityDefaults || !huntingDefaults || !dungeonDefaults)
    {
        return world;
    }

    GeneratedZone city;
    city.id = "starter_city";
    city.name = "Starter City";
    city.role = "starter_city";
    city.zoneType = "city";
    city.biome = cityDefaults->biome;
    city.tileStyle = cityDefaults->tileStyle;
    city.width = cityTemplate->width;
    city.height = cityTemplate->height;
    city.isSafe = cityDefaults->isSafe;
    world.zones.push_back(std::move(city));

    GeneratedZone hunting;
    hunting.id = "starter_hunting";
    hunting.name = "Hunting Grounds";
    hunting.role = "hunting";
    hunting.zoneType = "hunting";
    hunting.biome = huntingDefaults->biome;
    hunting.tileStyle = huntingDefaults->tileStyle;
    hunting.width = huntingTemplate->width;
    hunting.height = huntingTemplate->height;
    hunting.isSafe = huntingDefaults->isSafe;
    world.zones.push_back(std::move(hunting));

    GeneratedZone dungeon;
    dungeon.id = "starter_dungeon";
    dungeon.name = "Goblin Cave";
    dungeon.role = "dungeon";
    dungeon.zoneType = "dungeon";
    dungeon.biome = dungeonDefaults->biome;
    dungeon.tileStyle = dungeonDefaults->tileStyle;
    dungeon.width = dungeonTemplate->width;
    dungeon.height = dungeonTemplate->height;
    dungeon.isSafe = dungeonDefaults->isSafe;
    world.zones.push_back(std::move(dungeon));

    world.links.push_back(
        ZoneLink{
            "city_to_hunting",
            "starter_city",
            "starter_hunting",
            "north",
            "south",
            "To Hunting Grounds"});
    world.links.push_back(
        ZoneLink{
            "hunting_to_city",
            "starter_hunting",
            "starter_city",
            "south",
            "north",
            "To Starter City"});
    world.links.push_back(
        ZoneLink{
            "hunting_to_dungeon",
            "starter_hunting",
            "starter_dungeon",
            "east",
            "west",
            "To Goblin Cave"});
    world.links.push_back(
        ZoneLink{
            "dungeon_to_hunting",
            "starter_dungeon",
            "starter_hunting",
            "west",
            "east",
            "Exit to Hunting Grounds"});

    return world;
}

} // namespace tbeq::worldgen
