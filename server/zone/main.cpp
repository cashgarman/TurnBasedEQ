#include "ZoneServer.hpp"

#include <iostream>
#include <string>

#include <asio.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/core/Config.hpp"
#include "tbeq/core/Log.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    auto config = tbeq::parseServerArgs(argc, argv, "zone_server");
    const std::string logName = "zone_" + config.zoneId;
    tbeq::initLogging(logName);

#ifdef _WIN32
    const std::string consoleTitle = "TBEQ Zone: " + config.zoneId;
    SetConsoleTitleA(consoleTitle.c_str());
#endif

    try
    {
        asio::io_context io;
        tbeq::server::ZoneServer app(io, config);
        std::cout << "TBEQ_ZONE_ID=" << config.zoneId << std::endl;
        std::cout << "TBEQ_ZONE_CLIENT_PORT=" << app.clientPort() << std::endl;

        if (!app.connectAndRegister())
        {
            spdlog::error("Failed to register with WorldLogin");
            return 1;
        }

        spdlog::info("ZoneServer registered successfully for zone {}", config.zoneId);
        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("ZoneServer fatal error: {}", ex.what());
        return 1;
    }

    return 0;
}
