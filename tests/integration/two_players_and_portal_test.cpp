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

struct LoginHandoffResult
{
    uint64_t sessionTokenHash = 0;
    std::string characterId;
    tbeq::net::ZoneConnectInfoPayload zoneConnect;
};

std::optional<tbeq::net::SerializedPacket> readClientPacketOfType(
    tbeq::test::HeadlessClient& client,
    tbeq::net::ClientPacketType type,
    std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds(0))
        {
            break;
        }

        const auto packet = client.readClientPacket(remaining);
        if (!packet.has_value())
        {
            continue;
        }
        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType) == type)
        {
            return packet;
        }
    }
    return std::nullopt;
}

std::optional<LoginHandoffResult> loginCreateAndHandoff(
    tbeq::test::HeadlessClient& client,
    const std::string& username,
    const std::string& password,
    const std::string& characterName)
{
    LoginHandoffResult result;

    tbeq::net::CreateAccountRequestPayload createAccount;
    createAccount.username = username;
    createAccount.password = password;
    client.sendClientPacket(
        tbeq::net::ClientPacketType::CreateAccountRequest,
        1,
        tbeq::net::serialize(createAccount));

    const auto createAccountResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    if (!createAccountResponsePacket.has_value())
    {
        return std::nullopt;
    }

    tbeq::net::LoginRequestPayload login;
    login.username = username;
    login.password = password;
    client.sendClientPacket(tbeq::net::ClientPacketType::LoginRequest, 2, tbeq::net::serialize(login));

    const auto loginResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    if (!loginResponsePacket.has_value())
    {
        return std::nullopt;
    }

    tbeq::net::LoginResponsePayload loginResponse;
    if (!tbeq::net::deserializeClientPacket(*loginResponsePacket, loginResponse) || !loginResponse.ok)
    {
        return std::nullopt;
    }
    result.sessionTokenHash = loginResponse.sessionTokenHash;

    tbeq::net::CreateCharacterRequestPayload createCharacter;
    createCharacter.name = characterName;
    createCharacter.raceId = "human";
    createCharacter.classId = "warrior";
    client.sendClientPacket(
        tbeq::net::ClientPacketType::CreateCharacterRequest,
        3,
        tbeq::net::serialize(createCharacter),
        loginResponse.sessionTokenHash);

    const auto createCharacterResponsePacket = client.readClientPacket(std::chrono::seconds(3));
    if (!createCharacterResponsePacket.has_value())
    {
        return std::nullopt;
    }

    tbeq::net::CreateCharacterResponsePayload createCharacterResponse;
    if (!tbeq::net::deserializeClientPacket(*createCharacterResponsePacket, createCharacterResponse)
        || !createCharacterResponse.ok)
    {
        return std::nullopt;
    }
    result.characterId = createCharacterResponse.characterId;

    tbeq::net::SelectCharacterRequestPayload selectCharacter;
    selectCharacter.characterId = result.characterId;
    client.sendClientPacket(
        tbeq::net::ClientPacketType::SelectCharacterRequest,
        4,
        tbeq::net::serialize(selectCharacter),
        loginResponse.sessionTokenHash);

    const auto zoneConnectPacket = client.readClientPacket(std::chrono::seconds(5));
    if (!zoneConnectPacket.has_value())
    {
        return std::nullopt;
    }

    if (!tbeq::net::deserializeClientPacket(*zoneConnectPacket, result.zoneConnect) || !result.zoneConnect.ok)
    {
        return std::nullopt;
    }

    return result;
}

bool sessionResumeAndReadSnapshot(
    tbeq::test::HeadlessClient& client,
    const LoginHandoffResult& login,
    tbeq::net::ZoneSnapshotPayload& snapshot,
    int32_t* tileX = nullptr,
    int32_t* tileY = nullptr)
{
    client.close();
    client.connect("127.0.0.1", login.zoneConnect.zonePort);

    tbeq::net::SessionResumePayload resume;
    resume.characterId = login.characterId;
    resume.sessionResumeToken = login.sessionTokenHash;
    client.sendClientPacket(
        tbeq::net::ClientPacketType::SessionResume,
        10,
        tbeq::net::serialize(resume),
        login.sessionTokenHash);

    bool gotSnapshot = false;
    bool resumeOk = false;
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        const auto packet = client.readClientPacket(std::chrono::seconds(3));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::SessionResumeResponse)
        {
            tbeq::net::SessionResumeResponsePayload response;
            if (!tbeq::net::deserializeClientPacket(*packet, response) || !response.ok)
            {
                return false;
            }
            resumeOk = true;
            if (tileX != nullptr)
            {
                *tileX = response.tileX;
            }
            if (tileY != nullptr)
            {
                *tileY = response.tileY;
            }
        }
        else if (type == tbeq::net::ClientPacketType::ZoneSnapshot)
        {
            if (!tbeq::net::deserializeClientPacket(*packet, snapshot))
            {
                return false;
            }
            gotSnapshot = true;
            if (resumeOk)
            {
                return true;
            }
        }
    }

    return gotSnapshot && resumeOk;
}

bool readMoveResult(
    tbeq::test::HeadlessClient& client,
    tbeq::net::MoveResultPayload& moveResult,
    std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds(0))
        {
            break;
        }

        const auto packet = client.readClientPacket(remaining);
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::MoveResult)
        {
            return tbeq::net::deserializeClientPacket(*packet, moveResult);
        }
    }

    return false;
}

} // namespace

TEST_CASE("two players see each other after movement", "[integration][movement]")
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
    tbeq::test::HeadlessClient clientA(io);
    clientA.connect("127.0.0.1", cluster.worldLoginClientPort());
    const auto loginAOpt = loginCreateAndHandoff(clientA, "player_a_2p", "password_a_2p", "HeroA_2p");
    REQUIRE(loginAOpt.has_value());
    const auto loginA = *loginAOpt;

    tbeq::net::ZoneSnapshotPayload snapshotA;
    REQUIRE(sessionResumeAndReadSnapshot(clientA, loginA, snapshotA));

    tbeq::test::HeadlessClient clientB(io);
    clientB.connect("127.0.0.1", cluster.worldLoginClientPort());
    const auto loginBOpt = loginCreateAndHandoff(clientB, "player_b_2p", "password_b_2p", "HeroB_2p");
    REQUIRE(loginBOpt.has_value());
    const auto loginB = *loginBOpt;
    tbeq::net::ZoneSnapshotPayload snapshotB;
    REQUIRE(sessionResumeAndReadSnapshot(clientB, loginB, snapshotB));

    tbeq::net::MoveIntentPayload move;
    move.targetTileX = 32;
    move.targetTileY = 33;
    clientA.sendClientPacket(
        tbeq::net::ClientPacketType::MoveIntent,
        20,
        tbeq::net::serialize(move),
        loginA.sessionTokenHash);

    tbeq::net::MoveResultPayload moveResult;
    REQUIRE(readMoveResult(clientA, moveResult));
    REQUIRE(moveResult.ok);

    bool sawHeroA = false;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const auto entityPacket = clientB.readClientPacket(std::chrono::seconds(3));
        if (!entityPacket.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(entityPacket->header.packetType)
            != tbeq::net::ClientPacketType::EntitySnapshot)
        {
            continue;
        }

        tbeq::net::EntitySnapshotPayload entities;
        REQUIRE(tbeq::net::deserializeClientPacket(*entityPacket, entities));
        for (const auto& entity : entities.entities)
        {
            if (entity.name == "HeroA_2p" && entity.tileY == 33)
            {
                sawHeroA = true;
                break;
            }
        }
    }

    REQUIRE(sawHeroA);
}

TEST_CASE("zone portal transfer reaches destination zone", "[integration][movement]")
{
    if (!serverExecutablesExist())
    {
        SKIP("Server executables not built yet");
    }

    tbeq::test::TestCluster cluster;
    REQUIRE(cluster.waitForWorldLoginReady(std::chrono::seconds(8)));
    REQUIRE(cluster.waitForWorldLoginClientReady(std::chrono::seconds(8)));

    tbeq::test::ProcessHandle cityZoneProcess;
    tbeq::test::ProcessHandle huntingZoneProcess;
    const uint16_t cityZonePort = tbeq::test::TestCluster::pickEphemeralPort();
    const uint16_t huntingZonePort = tbeq::test::TestCluster::pickEphemeralPort();
    REQUIRE(cluster.startZoneServer("starter_city", cityZonePort, cityZoneProcess));
    REQUIRE(cluster.startZoneServer("starter_hunting", huntingZonePort, huntingZoneProcess));

    asio::io_context io;
    tbeq::test::HeadlessClient client(io);
    client.connect("127.0.0.1", cluster.worldLoginClientPort());
    const auto loginOpt = loginCreateAndHandoff(client, "portal_user_p2", "portal_pass_p2", "PortalHeroP2");
    REQUIRE(loginOpt.has_value());
    auto login = *loginOpt;

    tbeq::net::ZoneSnapshotPayload snapshot;
    int32_t currentX = 0;
    int32_t currentY = 0;
    REQUIRE(sessionResumeAndReadSnapshot(client, login, snapshot, &currentX, &currentY));
    REQUIRE(snapshot.zoneId == "starter_city");

    constexpr int32_t kPortalTileY = 8;
    for (int step = 0; currentY > kPortalTileY && step < 64; ++step)
    {
        const int32_t previousY = currentY;
        tbeq::net::MoveIntentPayload move;
        move.targetTileX = currentX;
        move.targetTileY = currentY - 1;
        client.sendClientPacket(
            tbeq::net::ClientPacketType::MoveIntent,
            100 + static_cast<uint32_t>(currentY),
            tbeq::net::serialize(move),
            login.sessionTokenHash);

        tbeq::net::MoveResultPayload moveResult;
        REQUIRE(readMoveResult(client, moveResult));
        REQUIRE(moveResult.ok);
        REQUIRE(moveResult.tileY < previousY);
        currentY = moveResult.tileY;
        currentX = moveResult.tileX;
    }
    REQUIRE(currentY == kPortalTileY);

    client.sendClientPacket(
        tbeq::net::ClientPacketType::UsePortal,
        200,
        tbeq::net::serialize(tbeq::net::UsePortalPayload{}),
        login.sessionTokenHash);

    const auto transferPacket = readClientPacketOfType(
        client,
        tbeq::net::ClientPacketType::ZoneConnectInfo,
        std::chrono::seconds(15));
    REQUIRE(transferPacket.has_value());

    tbeq::net::ZoneConnectInfoPayload transferConnect;
    REQUIRE(tbeq::net::deserializeClientPacket(*transferPacket, transferConnect));
    REQUIRE(transferConnect.ok);
    REQUIRE(transferConnect.zoneId == "starter_hunting");
    REQUIRE(transferConnect.zonePort == huntingZonePort);

    login.zoneConnect = transferConnect;
    tbeq::net::ZoneSnapshotPayload huntingSnapshot;
    REQUIRE(sessionResumeAndReadSnapshot(client, login, huntingSnapshot));
    REQUIRE(huntingSnapshot.zoneId == "starter_hunting");
}
