#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <filesystem>
#include <thread>

#include "HeadlessClient.hpp"
#include "TestCluster.hpp"
#include "tbeq/net/ClientPackets.hpp"

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

TEST_CASE("login and character handoff reaches zone server", "[integration][auth]")
{
    if (!serverExecutablesExist())
    {
        SKIP("Server executables not built yet");
    }

    tbeq::test::TestCluster cluster;
    REQUIRE(cluster.waitForWorldLoginReady(std::chrono::seconds(8)));
    REQUIRE(cluster.waitForWorldLoginClientReady(std::chrono::seconds(8)));

    tbeq::test::ProcessHandle zoneProcess;
    const uint16_t zoneClientPort = tbeq::test::TestCluster::pickEphemeralPort();
    REQUIRE(cluster.startZoneServer("starter_city", zoneClientPort, zoneProcess));

    asio::io_context io;
    tbeq::test::HeadlessClient client(io);
    client.connect("127.0.0.1", cluster.worldLoginClientPort());

    tbeq::net::CreateAccountRequestPayload createAccount;
    createAccount.username = "integration_user";
    createAccount.password = "integration_pass";
    client.sendClientPacket(tbeq::net::ClientPacketType::CreateAccountRequest, 1, tbeq::net::serialize(createAccount));

    const auto createAccountResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    REQUIRE(createAccountResponsePacket.has_value());
    tbeq::net::CreateAccountResponsePayload createAccountResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*createAccountResponsePacket, createAccountResponse));
    REQUIRE(createAccountResponse.ok);

    tbeq::net::LoginRequestPayload login;
    login.username = createAccount.username;
    login.password = createAccount.password;
    client.sendClientPacket(tbeq::net::ClientPacketType::LoginRequest, 2, tbeq::net::serialize(login));

    const auto loginResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    REQUIRE(loginResponsePacket.has_value());
    tbeq::net::LoginResponsePayload loginResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*loginResponsePacket, loginResponse));
    REQUIRE(loginResponse.ok);
    REQUIRE(loginResponse.sessionTokenHash != 0);

    tbeq::net::CreateCharacterRequestPayload createCharacter;
    createCharacter.name = "IntegrationHero";
    createCharacter.raceId = "human";
    createCharacter.classId = "warrior";
    client.sendClientPacket(
        tbeq::net::ClientPacketType::CreateCharacterRequest,
        3,
        tbeq::net::serialize(createCharacter),
        loginResponse.sessionTokenHash);

    const auto createCharacterResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    REQUIRE(createCharacterResponsePacket.has_value());
    tbeq::net::CreateCharacterResponsePayload createCharacterResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*createCharacterResponsePacket, createCharacterResponse));
    REQUIRE(createCharacterResponse.ok);
    REQUIRE_FALSE(createCharacterResponse.characterId.empty());

    tbeq::net::SelectCharacterRequestPayload selectCharacter;
    selectCharacter.characterId = createCharacterResponse.characterId;
    client.sendClientPacket(
        tbeq::net::ClientPacketType::SelectCharacterRequest,
        4,
        tbeq::net::serialize(selectCharacter),
        loginResponse.sessionTokenHash);

    const auto zoneConnectPacket = client.readClientPacket(std::chrono::seconds(5));
    REQUIRE(zoneConnectPacket.has_value());
    tbeq::net::ZoneConnectInfoPayload zoneConnect;
    REQUIRE(tbeq::net::deserializeClientPacket(*zoneConnectPacket, zoneConnect));
    REQUIRE(zoneConnect.ok);
    REQUIRE(zoneConnect.zoneId == "starter_city");
    REQUIRE(zoneConnect.zonePort == zoneClientPort);
    REQUIRE(zoneConnect.sessionResumeToken == loginResponse.sessionTokenHash);

    client.close();
}
