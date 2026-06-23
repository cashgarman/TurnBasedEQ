#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tbeq
{

struct AppConfig
{
    bool devMode = false;
    std::string zoneId = "starter";
    uint16_t worldLoginPort = 9000;
    uint16_t worldLoginClientPort = 9001;
    uint16_t zoneClientPort = 9100;
    std::string worldLoginHost = "127.0.0.1";
    std::string dataRoot = "data";
    std::string configRoot = "config";
    std::string dbPath = "config/tbeq.db";
    int64_t worldSeed = 42;
};

AppConfig parseServerArgs(int argc, char** argv, const std::string& defaultProcessName);

} // namespace tbeq
