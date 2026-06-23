#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <filesystem>

#include "HeadlessClient.hpp"
#include "TestCluster.hpp"
#include "tbeq/net/ServerPackets.hpp"

namespace
{

bool serverExecutablesExist()
{
    const std::filesystem::path repoRoot = TBEQ_REPO_ROOT;
    const auto debugWorldLogin = repoRoot / "build" / "server" / "Debug" / "tbeq_world_login.exe";
    const auto releaseWorldLogin = repoRoot / "build" / "server" / "Release" / "tbeq_world_login.exe";
    return std::filesystem::exists(debugWorldLogin) || std::filesystem::exists(releaseWorldLogin);
}

} // namespace

TEST_CASE("cluster registers zone with WorldLogin", "[integration][cluster]")
{
    if (!serverExecutablesExist())
    {
        SKIP("Server executables not built yet");
    }

    tbeq::test::TestCluster cluster;
    REQUIRE(cluster.waitForWorldLoginReady(std::chrono::seconds(5)));

    asio::io_context io;
    tbeq::test::HeadlessClient client(io);
    client.connect("127.0.0.1", cluster.worldLoginPort());

    tbeq::net::ZoneRegisterPayload payload;
    payload.zoneId = "integration_zone";
    payload.listenHost = "127.0.0.1";
    payload.listenPort = 9200;
    payload.capacity = 50;
    payload.version = "test";

    const auto writer = tbeq::net::serialize(payload);
    client.sendServerPacket(tbeq::net::ServerPacketType::ZoneRegister, 1, writer);

    const auto response = client.readServerPacket(std::chrono::seconds(3));
    REQUIRE(response.has_value());

    tbeq::net::ZoneRegisterAckPayload ack;
    REQUIRE(tbeq::net::deserializeServerPacket(*response, ack));
    REQUIRE(ack.accepted);
    REQUIRE_FALSE(ack.message.empty());

    client.close();
}
