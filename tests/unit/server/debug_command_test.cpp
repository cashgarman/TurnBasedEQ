#include <catch2/catch_test_macros.hpp>

#include "debug/DebugCommandHandler.hpp"

TEST_CASE("Debug command rejected when dev-mode off", "[server][debug]")
{
    tbeq::server::DebugCommandHandler handler(false);
    tbeq::net::DebugCommandRequestPayload request;
    request.command = tbeq::net::DebugCommand::Ping;

    const auto result = handler.handle(request);
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.message.find("dev-mode") != std::string::npos);
}

TEST_CASE("Debug command accepted when dev-mode on", "[server][debug]")
{
    tbeq::server::DebugCommandHandler handler(true);
    tbeq::net::DebugCommandRequestPayload request;
    request.command = tbeq::net::DebugCommand::Ping;

    const auto result = handler.handle(request);
    REQUIRE(result.ok);
}
