#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace
{

bool validateArrayFile(const fs::path& path, const std::vector<std::string>& requiredFields)
{
    std::ifstream input(path);
    if (!input)
    {
        std::cerr << "Failed to open " << path << "\n";
        return false;
    }

    nlohmann::json root;
    input >> root;
    if (!root.is_array())
    {
        std::cerr << path << " must be a JSON array\n";
        return false;
    }

    for (const auto& entry : root)
    {
        for (const auto& field : requiredFields)
        {
            if (!entry.contains(field))
            {
                std::cerr << path << " entry missing field: " << field << "\n";
                return false;
            }
        }
    }

    return true;
}

bool validateObjectFile(const fs::path& path, const std::vector<std::string>& requiredFields)
{
    std::ifstream input(path);
    if (!input)
    {
        std::cerr << "Failed to open " << path << "\n";
        return false;
    }

    nlohmann::json root;
    input >> root;
    if (!root.is_object())
    {
        std::cerr << path << " must be a JSON object\n";
        return false;
    }

    for (const auto& field : requiredFields)
    {
        if (!root.contains(field))
        {
            std::cerr << path << " missing field: " << field << "\n";
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: tbeq_data_validator <data_root>\n";
        return 1;
    }

    const fs::path dataRoot = argv[1];
    bool ok = true;

    ok &= validateArrayFile(dataRoot / "races.json", {"id", "name", "allowedClasses", "startZone"});
    ok &= validateArrayFile(dataRoot / "classes.json", {"id", "name"});
    ok &= validateArrayFile(dataRoot / "spells.json", {"id", "name", "classId", "manaCost", "school", "targetType", "effect", "baseValue"});
    ok &= validateArrayFile(dataRoot / "abilities.json", {"id", "name", "classId", "targetType", "effect", "baseValue"});
    {
        std::ifstream aiInput(dataRoot / "ai/class_combat_profiles.json");
        if (!aiInput)
        {
            std::cerr << "Failed to open ai/class_combat_profiles.json\n";
            ok = false;
        }
        else
        {
            nlohmann::json aiRoot;
            aiInput >> aiRoot;
            if (!aiRoot.is_object() || aiRoot.empty())
            {
                std::cerr << "ai/class_combat_profiles.json must be a non-empty object\n";
                ok = false;
            }
        }
    }
    ok &= validateArrayFile(dataRoot / "tile_defs.json", {"id", "category", "collision"});
    ok &= validateArrayFile(dataRoot / "tile_styles.json", {"id", "masterSeed", "palette"});
    ok &= validateObjectFile(dataRoot / "entity_sprites.json", {"races", "classes", "npcRoles", "defaults"});
    ok &= validateArrayFile(dataRoot / "items.json", {"id", "name"});
    ok &= validateArrayFile(dataRoot / "npcs.json", {"id", "name", "roles"});
    ok &= validateObjectFile(dataRoot / "worldgen/world_rules.json", {"zoneRoles", "validatorRules"});
    ok &= validateObjectFile(dataRoot / "worldgen/zone_templates.json", {"city", "hunting", "dungeon"});

    if (!ok)
    {
        std::cerr << "Content validation failed\n";
        return 1;
    }

    std::cout << "Content validation passed\n";
    return 0;
}
