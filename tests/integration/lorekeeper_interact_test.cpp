#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

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
    createAccount.password = "lore_pass";
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

} // namespace

TEST_CASE("lorekeeper interact opens npc dialog", "[integration][npc]")
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
        "lore_user",
        "LoreHero",
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
    int32_t playerTileX = 0;
    int32_t playerTileY = 0;
    uint32_t loreNpcEntityId = 0;
    int32_t loreTileX = 0;
    int32_t loreTileY = 0;
    const auto resumeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < resumeDeadline)
    {
        if (gotResume && loreNpcEntityId != 0)
        {
            break;
        }

        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(500));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::SessionResumeResponse)
        {
            tbeq::net::SessionResumeResponsePayload response;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, response));
            REQUIRE(response.ok);
            gotResume = true;
            playerTileX = response.tileX;
            playerTileY = response.tileY;
        }
        else if (type == tbeq::net::ClientPacketType::EntitySnapshot)
        {
            tbeq::net::EntitySnapshotPayload entities;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, entities));
            for (const auto& entity : entities.entities)
            {
                if (entity.entityType != 0 && entity.appearanceId == "lore")
                {
                    loreNpcEntityId = entity.entityId;
                    loreTileX = entity.tileX;
                    loreTileY = entity.tileY;
                    break;
                }
            }
        }
    }
    REQUIRE(gotResume);

    if (loreNpcEntityId == 0)
    {
        SKIP("No lorekeeper NPC entities found in starter_city snapshot");
    }

    bool moveOk = false;
    for (int step = 0; step < 80; ++step)
    {
        const int32_t distance = std::abs(playerTileX - loreTileX) + std::abs(playerTileY - loreTileY);
        if (distance <= 1)
        {
            moveOk = true;
            break;
        }

        int32_t nextX = playerTileX;
        int32_t nextY = playerTileY;
        if (playerTileX < loreTileX)
        {
            ++nextX;
        }
        else if (playerTileX > loreTileX)
        {
            --nextX;
        }
        else if (playerTileY < loreTileY)
        {
            ++nextY;
        }
        else
        {
            --nextY;
        }

        tbeq::net::MoveIntentPayload move;
        move.targetTileX = nextX;
        move.targetTileY = nextY;
        zoneClient.sendClientPacket(
            tbeq::net::ClientPacketType::MoveIntent,
            static_cast<uint32_t>(step + 2),
            tbeq::net::serialize(move),
            session.sessionTokenHash);

        const auto moveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < moveDeadline)
        {
            const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(200));
            if (!packet.has_value())
            {
                continue;
            }

            if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
                == tbeq::net::ClientPacketType::MoveResult)
            {
                tbeq::net::MoveResultPayload moveResult;
                REQUIRE(tbeq::net::deserializeClientPacket(*packet, moveResult));
                if (moveResult.ok)
                {
                    playerTileX = moveResult.tileX;
                    playerTileY = moveResult.tileY;
                }
                break;
            }
        }
    }
    REQUIRE(moveOk);

    tbeq::net::NpcInteractRequestPayload interact;
    interact.npcEntityId = loreNpcEntityId;
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::NpcInteractRequest,
        100,
        tbeq::net::serialize(interact),
        session.sessionTokenHash);

    std::vector<std::string> dialogLines;
    const auto loreDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < loreDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(300));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            != tbeq::net::ClientPacketType::NpcDialogOpen)
        {
            continue;
        }

        tbeq::net::NpcDialogOpenPayload dialog;
        REQUIRE(tbeq::net::deserializeClientPacket(*packet, dialog));
        REQUIRE(dialog.npcEntityId == loreNpcEntityId);
        REQUIRE(!dialog.npcName.empty());
        dialogLines = dialog.lines;
        break;
    }

    REQUIRE(!dialogLines.empty());
    bool hasLoreContent = false;
    for (const auto& line : dialogLines)
    {
        if (line.find("titans") != std::string::npos
            || line.find("Merchant") != std::string::npos
            || line.find("gear") != std::string::npos
            || line.find("tales") != std::string::npos)
        {
            hasLoreContent = true;
            break;
        }
    }
    REQUIRE(hasLoreContent);
}
