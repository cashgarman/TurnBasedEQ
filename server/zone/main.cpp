#include "ZoneServer.hpp"

#include <iostream>

#include <asio.hpp>
#include <spdlog/spdlog.h>

#include "tbeq/core/Config.hpp"
#include "tbeq/core/Log.hpp"

int main(int argc, char** argv)
{
    tbeq::initLogging("zone_server");
    auto config = tbeq::parseServerArgs(argc, argv, "zone_server");

    try
    {
        asio::io_context io;
        tbeq::server::ZoneServer app(io, config);
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
