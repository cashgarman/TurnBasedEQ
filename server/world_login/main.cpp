#include "WorldLoginServer.hpp"

#include <iostream>

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
    tbeq::initLogging("world_login");
    auto config = tbeq::parseServerArgs(argc, argv, "world_login");

#ifdef _WIN32
    SetConsoleTitleA("TBEQ WorldLogin");
#endif

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
