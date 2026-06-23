#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

#include "HeadlessClient.hpp"
#include "TestCluster.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/DebugCommands.hpp"

namespace
{

bool serverExecutablesExist()
{
    const std::filesystem::path repoRoot = TBEQ_REPO_ROOT;
    const auto debugWorldLogin = repoRoot / "build" / "server" / "Debug" / "tbeq_world_login.exe";
    const auto releaseWorldLogin = repoRoot / "build" / "server" / "Release" / "tbeq_world_login.exe";
    return std::filesystem::exists(debugWorldLogin) || std::filesystem::exists(releaseWorldLogin);
}

struct ZoneSession
{
    std::string characterId;
    uint64_t sessionTokenHash = 0;
    uint16_t zonePort = 0;
};

ZoneSession loginAndEnterZone(
    tbeq::test::HeadlessClient& worldClient,
    tbeq::test::TestCluster& cluster,
    const std::string& username,
    const std::string& characterName,
    const std::string& zoneId,
    uint16_t zonePort)
{
    (void)cluster;
    ZoneSession session;

    tbeq::net::CreateAccountRequestPayload createAccount;
    createAccount.username = username;
    createAccount.password = "stuck_pass";
    worldClient.sendClientPacket(
        tbeq::net::ClientPacketType::CreateAccountRequest,
        1,
        tbeq::net::serialize(createAccount));

    const auto createAccountResponsePacket = worldClient.readClientPacket(std::chrono::seconds(3));
    REQUIRE(createAccountResponsePacket.has_value());
    tbeq::net::CreateAccountResponsePayload createAccountResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*createAccountResponsePacket, createAccountResponse));
    REQUIRE(createAccountResponse.ok);

    tbeq::net::LoginRequestPayload login;
    login.username = username;
    login.password = createAccount.password;
    worldClient.sendClientPacket(tbeq::net::ClientPacketType::LoginRequest, 2, tbeq::net::serialize(login));

    const auto loginResponsePacket = worldClient.readClientPacket(std::chrono::seconds(3));
    REQUIRE(loginResponsePacket.has_value());
    tbeq::net::LoginResponsePayload loginResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*loginResponsePacket, loginResponse));
    REQUIRE(loginResponse.ok);

    tbeq::net::CreateCharacterRequestPayload createCharacter;
    createCharacter.name = characterName;
    createCharacter.raceId = "human";
    createCharacter.classId = "warrior";
    worldClient.sendClientPacket(
        tbeq::net::ClientPacketType::CreateCharacterRequest,
        3,
        tbeq::net::serialize(createCharacter),
        loginResponse.sessionTokenHash);

    const auto createCharacterResponsePacket = worldClient.readClientPacket(std::chrono::seconds(3));
    REQUIRE(createCharacterResponsePacket.has_value());
    tbeq::net::CreateCharacterResponsePayload createCharacterResponse;
    REQUIRE(tbeq::net::deserializeClientPacket(*createCharacterResponsePacket, createCharacterResponse));
    REQUIRE(createCharacterResponse.ok);

    tbeq::net::SelectCharacterRequestPayload selectCharacter;
    selectCharacter.characterId = createCharacterResponse.characterId;
    worldClient.sendClientPacket(
        tbeq::net::ClientPacketType::SelectCharacterRequest,
        4,
        tbeq::net::serialize(selectCharacter),
        loginResponse.sessionTokenHash);

    const auto zoneConnectPacket = worldClient.readClientPacket(std::chrono::seconds(5));
    REQUIRE(zoneConnectPacket.has_value());
    tbeq::net::ZoneConnectInfoPayload zoneConnect;
    REQUIRE(tbeq::net::deserializeClientPacket(*zoneConnectPacket, zoneConnect));
    REQUIRE(zoneConnect.ok);
    REQUIRE(zoneConnect.zoneId == zoneId);
    REQUIRE(zoneConnect.zonePort == zonePort);

    session.characterId = createCharacterResponse.characterId;
    session.sessionTokenHash = zoneConnect.sessionResumeToken;
    session.zonePort = zonePort;
    return session;
}

bool resumeZoneSession(tbeq::test::HeadlessClient& zoneClient, const ZoneSession& session)
{
    tbeq::net::SessionResumePayload resume;
    resume.characterId = session.characterId;
    resume.sessionResumeToken = session.sessionTokenHash;
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::SessionResume,
        1,
        tbeq::net::serialize(resume),
        session.sessionTokenHash);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::SessionResumeResponse)
        {
            tbeq::net::SessionResumeResponsePayload response;
            return tbeq::net::deserializeClientPacket(*packet, response) && response.ok;
        }
    }

    return false;
}

} // namespace

TEST_CASE("stuck command teleports player to zone center", "[integration][commands]")
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
    tbeq::test::HeadlessClient worldClient(io);
    worldClient.connect("127.0.0.1", cluster.worldLoginClientPort());

    const ZoneSession session = loginAndEnterZone(
        worldClient,
        cluster,
        "stuck_user",
        "StuckHero",
        "starter_city",
        zoneClientPort);
    worldClient.close();

    tbeq::test::HeadlessClient zoneClient(io);
    zoneClient.connect("127.0.0.1", zoneClientPort);
    REQUIRE(resumeZoneSession(zoneClient, session));

    tbeq::net::PlayerCommandRequestPayload stuckCommand;
    stuckCommand.command = "stuck";
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::PlayerCommandRequest,
        2,
        tbeq::net::serialize(stuckCommand),
        session.sessionTokenHash);

    bool gotResult = false;
    tbeq::net::PlayerCommandResultPayload result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!gotResult && std::chrono::steady_clock::now() < deadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::PlayerCommandResult)
        {
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, result));
            gotResult = true;
        }
    }

    REQUIRE(gotResult);
    REQUIRE(result.ok);
    REQUIRE(result.tileX == 32);
    REQUIRE(result.tileY == 32);
}

TEST_CASE("stuck command ends active combat and relocates player", "[integration][commands][combat]")
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
    tbeq::test::HeadlessClient worldClient(io);
    worldClient.connect("127.0.0.1", cluster.worldLoginClientPort());

    const ZoneSession session = loginAndEnterZone(
        worldClient,
        cluster,
        "stuck_combat_user",
        "StuckCombatHero",
        "starter_city",
        zoneClientPort);
    worldClient.close();

    tbeq::test::HeadlessClient zoneClient(io);
    zoneClient.connect("127.0.0.1", zoneClientPort);
    REQUIRE(resumeZoneSession(zoneClient, session));

    tbeq::net::DebugCommandRequestPayload spawnMob;
    spawnMob.command = tbeq::net::DebugCommand::SpawnMob;
    spawnMob.args = {"forest_rat"};
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::DebugCommandRequest,
        2,
        tbeq::net::serialize(spawnMob),
        session.sessionTokenHash);

    bool combatStarted = false;
    const auto combatDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!combatStarted && std::chrono::steady_clock::now() < combatDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::CombatStart)
        {
            combatStarted = true;
        }
    }
    REQUIRE(combatStarted);

    tbeq::net::PlayerCommandRequestPayload stuckCommand;
    stuckCommand.command = "stuck";
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::PlayerCommandRequest,
        3,
        tbeq::net::serialize(stuckCommand),
        session.sessionTokenHash);

    bool gotResult = false;
    bool gotCombatEnd = false;
    tbeq::net::PlayerCommandResultPayload result;
    const auto stuckDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((!gotResult || !gotCombatEnd) && std::chrono::steady_clock::now() < stuckDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::PlayerCommandResult)
        {
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, result));
            gotResult = true;
        }
        else if (type == tbeq::net::ClientPacketType::CombatEnd)
        {
            gotCombatEnd = true;
        }
    }

    REQUIRE(gotResult);
    REQUIRE(gotCombatEnd);
    REQUIRE(result.ok);
    REQUIRE(result.tileX == 32);
    REQUIRE(result.tileY == 32);
}
