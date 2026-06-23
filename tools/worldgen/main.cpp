#include <filesystem>
#include <iostream>
#include <string>

#include <spdlog/spdlog.h>

#include "tbeq/core/Log.hpp"
#include "tbeq/persistence/Database.hpp"
#include "tbeq/worldgen/WorldGenerator.hpp"

namespace
{

struct CliOptions
{
    int64_t seed = 42;
    std::string dbPath = "config/tbeq.db";
    std::string dataRoot = "data";
    bool validateOnly = false;
    bool force = false;
};

CliOptions parseArgs(int argc, char** argv)
{
    CliOptions options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc)
        {
            options.seed = std::stoll(argv[++i]);
        }
        else if (arg == "--db-path" && i + 1 < argc)
        {
            options.dbPath = argv[++i];
        }
        else if (arg == "--data-root" && i + 1 < argc)
        {
            options.dataRoot = argv[++i];
        }
        else if (arg == "--validate-only")
        {
            options.validateOnly = true;
        }
        else if (arg == "--force")
        {
            options.force = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: tbeq_worldgen [--seed N] [--db-path PATH] [--data-root PATH] [--validate-only] [--force]\n";
            std::exit(0);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    tbeq::initLogging("worldgen");
    const auto options = parseArgs(argc, argv);
    const std::filesystem::path dataRoot = options.dataRoot;

    tbeq::worldgen::WorldGenerator generator(options.seed, dataRoot);
    if (options.validateOnly)
    {
        const auto report = generator.validateOnly();
        if (!report.ok)
        {
            for (const auto& error : report.errors)
            {
                spdlog::error("Validation error: {}", error);
            }
            return 1;
        }

        std::cout << "World validation passed for seed " << options.seed << std::endl;
        return 0;
    }

    tbeq::db::Database database(options.dbPath);
    if (!database.open() || !database.initializeSchema())
    {
        spdlog::error("Failed to open database {}", options.dbPath);
        return 1;
    }

    if (database.hasWorld() && !options.force)
    {
        spdlog::error("World already exists in database (use --force to regenerate)");
        return 1;
    }

    if (options.force && database.hasWorld())
    {
        if (!database.clearWorld())
        {
            spdlog::error("Failed to clear existing world");
            return 1;
        }
    }

    if (!generator.writeToDatabase(database))
    {
        spdlog::error("World generation failed");
        return 1;
    }

    std::cout << "World generated into " << options.dbPath << " with seed " << options.seed << std::endl;
    return 0;
}
