#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <chrono>
#include <cmath>
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
    createAccount.password = "merchant_pass";
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

std::optional<tbeq::net::InventorySnapshotPayload> waitForInventorySnapshot(
    tbeq::test::HeadlessClient& zoneClient,
    std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(300));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::InventorySnapshot)
        {
            tbeq::net::InventorySnapshotPayload inventory;
            if (tbeq::net::deserializeClientPacket(*packet, inventory))
            {
                return inventory;
            }
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("merchant buy grants item and deducts gold", "[integration][merchant]")
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
        "merchant_user",
        "MerchantHero",
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
    uint32_t merchantNpcEntityId = 0;
    int32_t merchantTileX = 0;
    int32_t merchantTileY = 0;
    std::optional<tbeq::net::InventorySnapshotPayload> initialInventory;
    const auto resumeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < resumeDeadline)
    {
        if (gotResume && merchantNpcEntityId != 0 && initialInventory.has_value())
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
                if (entity.entityType != 0 && entity.appearanceId == "merchant")
                {
                    merchantNpcEntityId = entity.entityId;
                    merchantTileX = entity.tileX;
                    merchantTileY = entity.tileY;
                    break;
                }
            }
        }
        else if (type == tbeq::net::ClientPacketType::InventorySnapshot)
        {
            tbeq::net::InventorySnapshotPayload inventory;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, inventory));
            initialInventory = std::move(inventory);
        }
    }
    REQUIRE(gotResume);
    REQUIRE(initialInventory.has_value());

    if (merchantNpcEntityId == 0)
    {
        SKIP("No NPC entities found in starter_city snapshot");
    }

    bool moveOk = false;
    for (int step = 0; step < 80; ++step)
    {
        const int32_t distance = std::abs(playerTileX - merchantTileX) + std::abs(playerTileY - merchantTileY);
        if (distance <= 1)
        {
            moveOk = true;
            break;
        }

        int32_t nextX = playerTileX;
        int32_t nextY = playerTileY;
        if (playerTileX < merchantTileX)
        {
            ++nextX;
        }
        else if (playerTileX > merchantTileX)
        {
            --nextX;
        }
        else if (playerTileY < merchantTileY)
        {
            ++nextY;
        }
        else if (playerTileY > merchantTileY)
        {
            --nextY;
        }

        tbeq::net::MoveIntentPayload moveIntent;
        moveIntent.targetTileX = nextX;
        moveIntent.targetTileY = nextY;
        zoneClient.sendClientPacket(
            tbeq::net::ClientPacketType::MoveIntent,
            static_cast<uint32_t>(step + 2),
            tbeq::net::serialize(moveIntent),
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
    interact.npcEntityId = merchantNpcEntityId;
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::NpcInteractRequest,
        4,
        tbeq::net::serialize(interact),
        session.sessionTokenHash);

    bool gotMerchantOpen = false;
    tbeq::net::MerchantOpenPayload merchantOpen;
    const auto openDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!gotMerchantOpen && std::chrono::steady_clock::now() < openDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(300));
        if (!packet.has_value())
        {
            continue;
        }

        if (static_cast<tbeq::net::ClientPacketType>(packet->header.packetType)
            == tbeq::net::ClientPacketType::MerchantOpen)
        {
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, merchantOpen));
            gotMerchantOpen = true;
        }
    }

    if (!gotMerchantOpen)
    {
        FAIL("Could not open merchant after moving adjacent to NPC");
    }

    REQUIRE(!merchantOpen.stock.empty());
    const std::string buyItemId = merchantOpen.stock.front().itemId;

    tbeq::net::MerchantBuyRequestPayload buyRequest;
    buyRequest.npcEntityId = merchantNpcEntityId;
    buyRequest.itemId = buyItemId;
    buyRequest.quantity = 1;
    zoneClient.sendClientPacket(
        tbeq::net::ClientPacketType::MerchantBuyRequest,
        5,
        tbeq::net::serialize(buyRequest),
        session.sessionTokenHash);

    bool gotBuyResult = false;
    tbeq::net::MerchantBuyResultPayload buyResult;
    std::optional<tbeq::net::InventorySnapshotPayload> finalInventory;
    const auto buyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!gotBuyResult && std::chrono::steady_clock::now() < buyDeadline)
    {
        const auto packet = zoneClient.readClientPacket(std::chrono::milliseconds(300));
        if (!packet.has_value())
        {
            continue;
        }

        const auto type = static_cast<tbeq::net::ClientPacketType>(packet->header.packetType);
        if (type == tbeq::net::ClientPacketType::MerchantBuyResult)
        {
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, buyResult));
            gotBuyResult = true;
        }
        else if (type == tbeq::net::ClientPacketType::InventorySnapshot)
        {
            tbeq::net::InventorySnapshotPayload inventory;
            REQUIRE(tbeq::net::deserializeClientPacket(*packet, inventory));
            finalInventory = std::move(inventory);
        }
    }

    REQUIRE(gotBuyResult);
    REQUIRE(buyResult.ok);
    if (!finalInventory.has_value())
    {
        finalInventory = waitForInventorySnapshot(zoneClient, std::chrono::seconds(3));
    }
    REQUIRE(finalInventory.has_value());
    REQUIRE(finalInventory->gold < initialInventory->gold);

    bool hasPurchasedItem = false;
    for (const auto& entry : finalInventory->items)
    {
        if (entry.itemId == buyItemId)
        {
            hasPurchasedItem = true;
            break;
        }
    }
    REQUIRE(hasPurchasedItem);
}
