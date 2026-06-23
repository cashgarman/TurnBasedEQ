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
    const std::string& classId,
    const std::string& zoneId,
    uint16_t zonePort)
{
    (void)cluster;
    ZoneSession session;

    tbeq::net::CreateAccountRequestPayload createAccount;
    createAccount.username = username;
    createAccount.password = "ai_combat_pass";
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
    createCharacter.classId = classId;
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

} // namespace

TEST_CASE("ai party combat with cleric companion resolves victory", "[integration][combat][ai]")
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
        "ai_combat_user",
        "AiHero",
        "warrior",
        "starter_city",
        zoneClientPort);
    worldClient.close();

    tbeq::test::HeadlessClient zoneClient(io);
    zoneClient.connect("127.0.0.1", zoneClientPort);

    tbeq::net::SessionResumePayload resume;
    resume.characterId = session.characterId;
    resume.sessionResumeToken = session.sessionTokenHash;
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::SessionResume,
        1,
        tbeq::net::serialize(resume),
        session.sessionTokenHash);

    bool gotResume = false;
    const auto resumeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!gotResume && std::chrono::steady_clock::now() < resumeDeadline)
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
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, response));
            REQUIRE(response.ok);
            gotResume = true;
        }
    }
    REQUIRE(gotResume);

    tbeq::net::DebugCommandRequestPayload spawnAi;
    spawnAi.command = tbeq::net::DebugCommand::SpawnAi;
    spawnAi.args = {"cleric", "5"};
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::DebugCommandRequest,
        2,
        tbeq::net::serialize(spawnAi),
        session.sessionTokenHash);

    bool aiSpawned = false;
    const auto spawnDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!aiSpawned && std::chrono::steady_clock::now() < spawnDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }
        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::DebugCommandResponse)
        {
            tbeq::net::DebugCommandResponsePayload response;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, response));
            REQUIRE(response.ok);
            aiSpawned = true;
        }
    }
    REQUIRE(aiSpawned);

    tbeq::net::DebugCommandRequestPayload spawnMob;
    spawnMob.command = tbeq::net::DebugCommand::SpawnMob;
    spawnMob.args = {"forest_rat"};
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::DebugCommandRequest,
        3,
        tbeq::net::serialize(spawnMob),
        session.sessionTokenHash);

    bool combatStarted = false;
    bool combatEnded = false;
    uint32_t combatId = 0;
    uint32_t playerSlot = 0;
    bool sawHeal = false;

    const auto combatDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < combatDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::CombatStart)
        {
            tbeq::net::CombatStartPayload start;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, start));
            combatStarted = true;
            combatId = start.combatId;
            for (const auto& participant : start.participants)
            {
                if (participant.isPlayerControlled)
                {
                    playerSlot = participant.combatSlot;
                }
            }
        }
        else if (type == tbeq::net::ClientPacketType::CombatEvent)
        {
            tbeq::net::CombatEventPayload event;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, event));
            if (event.eventType == static_cast<uint8_t>(11))
            {
                sawHeal = true;
            }
        }
        else if (type == tbeq::net::ClientPacketType::CombatEnd)
        {
            tbeq::net::CombatEndPayload endPayload;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, endPayload));
            combatEnded = endPayload.result == 1;
            break;
        }
        else if (type == tbeq::net::ClientPacketType::CombatUpdate && combatStarted)
        {
            tbeq::net::CombatUpdatePayload update;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, update));
            if (update.currentActorSlot == playerSlot)
            {
                tbeq::net::SubmitActionPayload action;
                action.combatId = combatId;
                action.actionType = 1;
                for (const auto& participant : update.participants)
                {
                    if (participant.side == 1 && participant.isAlive)
                    {
                        action.targetCombatSlot = participant.combatSlot;
                        break;
                    }
                }

                zoneClient.sendClientPacket(
                    tbeq::net::ClientPacketType::SubmitAction,
                    4,
                    tbeq::net::serialize(action),
                    session.sessionTokenHash);
            }
        }
    }

    REQUIRE(combatStarted);
    REQUIRE(combatEnded);
}
