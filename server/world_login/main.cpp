#include "WorldLoginServer.hpp"

#include <iostream>

#include <spdlog/spdlog.h>

#include "tbeq/core/Config.hpp"
#include "tbeq/core/Log.hpp"

int main(int argc, char** argv)
{
    tbeq::initLogging("world_login");
    auto config = tbeq::parseServerArgs(argc, argv, "world_login");

    try
    {
        asio::io_context io;
        tbeq::server::WorldLoginServer server(io, config);
        std::cout << "TBEQ_WORLD_LOGIN_PORT=" << server.zonePort() << std::endl;
        std::cout << "TBEQ_WORLD_LOGIN_CLIENT_PORT=" << server.clientPort() << std::endl;
        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("WorldLogin fatal error: {}", ex.what());
        return 1;
    }

    return 0;
}
