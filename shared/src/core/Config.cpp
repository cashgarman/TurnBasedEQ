#include "tbeq/core/Config.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace tbeq
{

AppConfig parseServerArgs(int argc, char** argv, const std::string& defaultProcessName)
{
    AppConfig config;
    (void)defaultProcessName;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--dev-mode")
        {
            config.devMode = true;
        }
        else if (arg == "--zone-id" && i + 1 < argc)
        {
            config.zoneId = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            config.worldLoginPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--client-port" && i + 1 < argc)
        {
            config.zoneClientPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--world-login" && i + 1 < argc)
        {
            config.worldLoginHost = argv[++i];
        }
        else if (arg == "--world-login-port" && i + 1 < argc)
        {
            config.worldLoginPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--data-root" && i + 1 < argc)
        {
            config.dataRoot = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0]
                      << " [--dev-mode] [--zone-id ID] [--port N] [--client-port N]"
                      << " [--world-login HOST] [--world-login-port N]\n";
            std::exit(0);
        }
    }

    return config;
}

} // namespace tbeq
