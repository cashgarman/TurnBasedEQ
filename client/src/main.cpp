#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>

#include "net/LocalClusterLauncher.hpp"
#include "net/LoginProfiler.hpp"
#include "net/ZoneClient.hpp"
#include "render/EntityRenderer.hpp"
#include "render/TilemapRenderer.hpp"
#include "render/procedural/SpriteGenerator.hpp"
#include "render/procedural/TileGenerator.hpp"
#include "tbeq/core/Log.hpp"
#include "tbeq/net/ClientPackets.hpp"
#include "tbeq/net/PacketSerializer.hpp"
#include "tbeq/social/ChatChannel.hpp"
#include "tbeq/world/TileDefCatalog.hpp"
#include "ui/GameWindow.hpp"
#include "ui/WindowLayoutManager.hpp"
#include "ui/combat/CombatWindow.hpp"
#include "ui/inventory/InventoryWindow.hpp"
#include "ui/merchant/MerchantWindow.hpp"
#include "tbeq/content/ItemCatalog.hpp"

#if TBEQ_ENABLE_DEBUG_MENU
#include "ui/debug/DebugMenu.hpp"
#include "ui/debug/LogViewer.hpp"
#endif

namespace
{

using LoginProfiler = tbeq::client::LoginProfiler;

void drawWorldNameplates(
    const std::vector<tbeq::client::EntityRenderer::WorldNameplate>& plates,
    int cameraTileX,
    int cameraTileY)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const int tileSize = tbeq::render::TileGenerator::kTileSize;

    for (const auto& plate : plates)
    {
        const float centerX = static_cast<float>((plate.tileX - cameraTileX) * tileSize + tileSize / 2);
        const float nameY = static_cast<float>((plate.tileY - cameraTileY) * tileSize - 4.0f);
        const float roleY = nameY - ImGui::GetTextLineHeight() - 2.0f;

        ImU32 roleColor = IM_COL32(220, 220, 220, 255);
        if (plate.appearanceId == "merchant")
        {
            roleColor = IM_COL32(255, 210, 80, 255);
        }
        else if (plate.appearanceId == "lore")
        {
            roleColor = IM_COL32(140, 180, 255, 255);
        }

        const ImVec2 nameSize = ImGui::CalcTextSize(plate.name.c_str());
        const ImVec2 roleSize = ImGui::CalcTextSize(plate.role.c_str());
        const float nameX = centerX - nameSize.x * 0.5f;
        const float roleX = centerX - roleSize.x * 0.5f;

        drawList->AddText(ImVec2(roleX + 1.0f, roleY + 1.0f), IM_COL32(0, 0, 0, 220), plate.role.c_str());
        drawList->AddText(ImVec2(roleX, roleY), roleColor, plate.role.c_str());
        drawList->AddText(ImVec2(nameX + 1.0f, nameY + 1.0f), IM_COL32(0, 0, 0, 220), plate.name.c_str());
        drawList->AddText(ImVec2(nameX, nameY), IM_COL32(255, 255, 255, 255), plate.name.c_str());
    }
}

enum class ClientState
{
    Login,
    InZone,
};

struct LoginSession
{
    std::string characterId;
    std::string characterName;
    std::string raceId = "human";
    std::string classId = "warrior";
    uint64_t sessionTokenHash = 0;
    tbeq::net::ZoneConnectInfoPayload zoneConnect;
};

struct LoginJobResult
{
    bool ok = false;
    std::string statusMessage;
    LoginSession session;
    std::string profileReport;
};

struct ClientApp
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 1280;
    int height = 720;
    bool running = true;
    ClientState state = ClientState::Login;

    asio::io_context io;
    tbeq::world::TileDefCatalog tileDefs;
    tbeq::content::ItemCatalog itemCatalog;
    tbeq::render::TileStyleCatalog styleCatalog;
    tbeq::render::EntitySpriteCatalog entitySpriteCatalog;
    std::unique_ptr<tbeq::client::ZoneClient> zoneClient;
    std::unique_ptr<tbeq::client::TilemapRenderer> tilemapRenderer;
    std::unique_ptr<tbeq::client::EntityRenderer> entityRenderer;
    tbeq::net::ZoneSnapshotPayload zoneSnapshot;
    LoginSession session;
    int32_t cameraTileX = 0;
    int32_t cameraTileY = 0;
    std::deque<std::string> chatLines;
    tbeq::client::LocalClusterLauncher localCluster;

    tbeq::ui::WindowLayoutManager layoutManager{"config/ui_layout.json"};
    tbeq::ui::GameWindow hudWindow{"hud", "HUD", 240.0f, 100.0f};
    tbeq::ui::GameWindow chatWindow{"chat", "Chat", 360.0f, 220.0f};
    tbeq::ui::GameWindow minimapWindow{"minimap", "Minimap", 160.0f, 160.0f};
    tbeq::ui::GameWindow combatWindowPanel{"combat", "Combat", 420.0f, 360.0f};
    tbeq::ui::GameWindow inventoryWindowPanel{"inventory", "Inventory", 360.0f, 320.0f};
    tbeq::ui::GameWindow merchantWindowPanel{"merchant", "Merchant", 400.0f, 340.0f};
    tbeq::client::CombatWindow combatWindow;
    tbeq::client::InventoryWindow inventoryWindow;
    tbeq::client::MerchantWindow merchantWindow;
    bool combatVisible = false;
    bool inventoryVisible = false;
    bool merchantVisible = false;
    uint32_t hudGold = 0;
    uint16_t hudHp = 100;
    uint16_t hudMaxHp = 100;
    uint16_t hudMana = 50;
    uint16_t hudMaxMana = 50;

#if TBEQ_ENABLE_DEBUG_MENU
    tbeq::ui::GameWindow debugWindow{"debug_menu", "Debug Menu", 400.0f, 300.0f};
    tbeq::ui::LogViewer logViewer;
    tbeq::ui::DebugMenu debugMenu{logViewer};
#endif

    char usernameBuffer[64] = "demo_user";
    char passwordBuffer[64] = "demo_pass";
    char characterNameBuffer[64] = "DemoHero";
    std::string statusMessage = "Login to enter the generated world.";
    bool loginInProgress = false;
    std::future<LoginJobResult> loginFuture;

    bool prepareEnvironment()
    {
        const auto repoRoot = tbeq::client::LocalClusterLauncher::detectRepoRoot();
        std::error_code ec;
        std::filesystem::current_path(repoRoot, ec);
        if (ec)
        {
            spdlog::warn("Could not set working directory to {}: {}", repoRoot.string(), ec.message());
        }
        else
        {
            spdlog::info("Working directory set to {}", repoRoot.string());
        }

        spdlog::info("Ensuring local WorldLogin and ZoneServer are running...");
        if (!localCluster.ensureRunning())
        {
            statusMessage = "Could not reach or start local servers. Build server targets first.";
            return false;
        }

        statusMessage = "Login to enter the generated world.";
        return true;
    }

    bool init()
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        {
            spdlog::error("SDL_Init failed: {}", SDL_GetError());
            return false;
        }

        window = SDL_CreateWindow(
            "TurnBasedEQ",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width,
            height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window)
        {
            spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer)
        {
            spdlog::error("SDL_CreateRenderer failed: {}", SDL_GetError());
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& ioImGui = ImGui::GetIO();
        ioImGui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ioImGui.IniFilename = nullptr;

        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        const auto dataRoot = std::filesystem::path("data");
        if (!tileDefs.loadFromFile(dataRoot / "tile_defs.json"))
        {
            spdlog::error("Failed to load tile_defs.json");
            return false;
        }
        if (!styleCatalog.loadFromFile(dataRoot / "tile_styles.json"))
        {
            spdlog::error("Failed to load tile_styles.json");
            return false;
        }
        if (!entitySpriteCatalog.loadFromFile(dataRoot / "entity_sprites.json"))
        {
            spdlog::error("Failed to load entity_sprites.json");
            return false;
        }
        if (!itemCatalog.loadFromFile(dataRoot / "items.json"))
        {
            spdlog::error("Failed to load items.json");
            return false;
        }

        inventoryWindow.setItemCatalog(&itemCatalog);
        merchantWindow.setItemCatalog(&itemCatalog);

        tilemapRenderer = std::make_unique<tbeq::client::TilemapRenderer>(renderer);
        entityRenderer = std::make_unique<tbeq::client::EntityRenderer>(renderer);

        layoutManager.registerWindow(hudWindow);
        layoutManager.registerWindow(chatWindow);
        layoutManager.registerWindow(minimapWindow);
        layoutManager.registerWindow(inventoryWindowPanel);
        layoutManager.registerWindow(merchantWindowPanel);
#if TBEQ_ENABLE_DEBUG_MENU
        layoutManager.registerWindow(debugWindow);
#endif
        layoutManager.load();

#if TBEQ_ENABLE_DEBUG_MENU
        logViewer.setSink(tbeq::getRingBufferSink());
#endif

        chatLines.push_back("[System] Welcome to TurnBasedEQ Phase 5. I inventory, N interact, P portal.");
        spdlog::info("TurnBasedEQ client started");
        return true;
    }

    void shutdown()
    {
        layoutManager.save();
        zoneClient.reset();
        localCluster.shutdownIfStarted();

        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        if (renderer)
        {
            SDL_DestroyRenderer(renderer);
        }
        if (window)
        {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    bool performLoginAndHandoff(LoginProfiler& profiler, LoginSession& outSession, std::string& outStatusMessage)
    {
        try
        {
            LoginProfiler::Scope connectScope(profiler, "world_login_connect");
            asio::ip::tcp::socket socket(io);
            asio::ip::tcp::resolver resolver(io);
            auto endpoints = resolver.resolve("127.0.0.1", std::to_string(tbeq::client::LocalClusterLauncher::kWorldLoginClientPort));
            asio::connect(socket, endpoints);
            std::error_code ec;
            socket.non_blocking(true, ec);
            spdlog::info("[login] connected to world login client port {}", tbeq::client::LocalClusterLauncher::kWorldLoginClientPort);

            auto sendPacket = [&](tbeq::net::ClientPacketType type, uint32_t seq, const tbeq::net::ByteWriter& writer, uint64_t tokenHash = 0)
            {
                auto packet = tbeq::net::serializeClientPacket(type, seq, writer);
                packet.header.sessionTokenHash = tokenHash;
                const auto encoded = tbeq::net::encodePacket(packet);
                asio::write(socket, asio::buffer(encoded));
                spdlog::info("[login] sent packet type={} seq={}", static_cast<uint16_t>(type), seq);
            };

            auto readPacket = [&](const char* stepName, int timeoutMs = 10000) -> std::optional<tbeq::net::SerializedPacket>
            {
                LoginProfiler::Scope readScope(profiler, stepName);
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

                tbeq::net::PacketHeader header{};
                while (std::chrono::steady_clock::now() < deadline)
                {
                    std::error_code readEc;
                    const std::size_t headerRead = socket.read_some(asio::buffer(&header, sizeof(header)), readEc);
                    if (headerRead == sizeof(header) && header.isValid())
                    {
                        break;
                    }
                    if (readEc == asio::error::would_block || readEc == asio::error::try_again)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }
                    if (readEc && headerRead == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }
                }

                if (!header.isValid())
                {
                    spdlog::error("[login] timed out waiting for packet header ({})", stepName);
                    return std::nullopt;
                }

                std::vector<uint8_t> payload(header.payloadLength);
                std::size_t received = 0;
                while (received < payload.size() && std::chrono::steady_clock::now() < deadline)
                {
                    std::error_code readEc;
                    const std::size_t payloadRead = socket.read_some(
                        asio::buffer(payload.data() + received, payload.size() - received),
                        readEc);
                    if (readEc == asio::error::would_block || readEc == asio::error::try_again)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }
                    if (readEc && payloadRead == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }
                    received += payloadRead;
                }

                if (received < payload.size())
                {
                    spdlog::error(
                        "[login] timed out reading payload for type={} ({} / {} bytes)",
                        header.packetType,
                        received,
                        payload.size());
                    return std::nullopt;
                }

                spdlog::info(
                    "[login] received packet type={} payloadBytes={}",
                    header.packetType,
                    payload.size());

                tbeq::net::SerializedPacket packet;
                packet.header = header;
                packet.payload = std::move(payload);
                return packet;
            };

            tbeq::net::LoginRequestPayload login;
            login.username = usernameBuffer;
            login.password = passwordBuffer;
            sendPacket(tbeq::net::ClientPacketType::LoginRequest, 1, tbeq::net::serialize(login));
            const auto loginPacket = readPacket("world_login_response");
            if (!loginPacket.has_value())
            {
                outStatusMessage = "No response from world login";
                return false;
            }

            tbeq::net::LoginResponsePayload loginResponse;
            if (!tbeq::net::deserializeClientPacket(*loginPacket, loginResponse) || !loginResponse.ok)
            {
                spdlog::info("[login] initial login failed, creating account for {}", usernameBuffer);
                tbeq::net::CreateAccountRequestPayload createAccount;
                createAccount.username = usernameBuffer;
                createAccount.password = passwordBuffer;
                sendPacket(
                    tbeq::net::ClientPacketType::CreateAccountRequest,
                    2,
                    tbeq::net::serialize(createAccount));
                (void)readPacket("create_account_response");

                sendPacket(tbeq::net::ClientPacketType::LoginRequest, 3, tbeq::net::serialize(login));
                const auto retryLoginPacket = readPacket("world_login_retry_response");
                if (!retryLoginPacket.has_value())
                {
                    outStatusMessage = "No response from world login after account create";
                    return false;
                }

                if (!tbeq::net::deserializeClientPacket(*retryLoginPacket, loginResponse) || !loginResponse.ok)
                {
                    outStatusMessage = loginResponse.message.empty() ? "Login failed" : loginResponse.message;
                    return false;
                }
            }

            spdlog::info("[login] authenticated sessionTokenHash={}", loginResponse.sessionTokenHash);

            std::string characterId;
            tbeq::net::CreateCharacterRequestPayload createCharacter;
            createCharacter.name = characterNameBuffer;
            createCharacter.raceId = "human";
            createCharacter.classId = "warrior";
            std::string selectedRaceId = createCharacter.raceId;
            std::string selectedClassId = createCharacter.classId;
            sendPacket(
                tbeq::net::ClientPacketType::CreateCharacterRequest,
                4,
                tbeq::net::serialize(createCharacter),
                loginResponse.sessionTokenHash);
            const auto createCharacterPacket = readPacket("create_character_response");
            if (!createCharacterPacket.has_value())
            {
                outStatusMessage = "No response from create character";
                return false;
            }

            tbeq::net::CreateCharacterResponsePayload createCharacterResponse;
            if (tbeq::net::deserializeClientPacket(*createCharacterPacket, createCharacterResponse)
                && createCharacterResponse.ok)
            {
                characterId = createCharacterResponse.characterId;
                spdlog::info("[login] created character {}", characterId);
            }
            else
            {
                spdlog::info("[login] create character failed, selecting existing character {}", characterNameBuffer);
                sendPacket(
                    tbeq::net::ClientPacketType::CharacterListRequest,
                    5,
                    tbeq::net::serialize(tbeq::net::CharacterListRequestPayload{}),
                    loginResponse.sessionTokenHash);
                const auto characterListPacket = readPacket("character_list_response");
                if (!characterListPacket.has_value())
                {
                    outStatusMessage = "No response from character list";
                    return false;
                }

                tbeq::net::CharacterListResponsePayload characterList;
                if (!tbeq::net::deserializeClientPacket(*characterListPacket, characterList) || !characterList.ok)
                {
                    outStatusMessage = characterList.message.empty() ? "Character create failed" : characterList.message;
                    return false;
                }

                for (const auto& character : characterList.characters)
                {
                    if (character.name == characterNameBuffer)
                    {
                        characterId = character.characterId;
                        selectedRaceId = character.raceId;
                        selectedClassId = character.classId;
                        break;
                    }
                }

                if (characterId.empty())
                {
                    outStatusMessage = createCharacterResponse.message.empty()
                        ? "Character create failed"
                        : createCharacterResponse.message;
                    return false;
                }

                spdlog::info("[login] selected existing character {}", characterId);
            }

            tbeq::net::SelectCharacterRequestPayload selectCharacter;
            selectCharacter.characterId = characterId;
            sendPacket(
                tbeq::net::ClientPacketType::SelectCharacterRequest,
                6,
                tbeq::net::serialize(selectCharacter),
                loginResponse.sessionTokenHash);

            const auto zoneConnectPacket = readPacket("zone_connect_response", 15000);
            if (!zoneConnectPacket.has_value())
            {
                outStatusMessage = "No zone handoff response";
                return false;
            }

            tbeq::net::ZoneConnectInfoPayload zoneConnect;
            if (!tbeq::net::deserializeClientPacket(*zoneConnectPacket, zoneConnect) || !zoneConnect.ok)
            {
                outStatusMessage = zoneConnect.message.empty() ? "Zone handoff failed" : zoneConnect.message;
                return false;
            }

            spdlog::info(
                "[login] zone handoff {}:{} zone={}",
                zoneConnect.zoneHost,
                zoneConnect.zonePort,
                zoneConnect.zoneId);

            outSession.characterId = zoneConnect.characterId;
            outSession.characterName = characterNameBuffer;
            outSession.raceId = selectedRaceId;
            outSession.classId = selectedClassId;
            outSession.sessionTokenHash = zoneConnect.sessionResumeToken;
            outSession.zoneConnect = zoneConnect;
            return true;
        }
        catch (const std::exception& ex)
        {
            outStatusMessage = ex.what();
            spdlog::error("[login] exception during world login handoff: {}", ex.what());
            return false;
        }
    }

    LoginJobResult runLoginJob()
    {
        LoginJobResult result;
        LoginProfiler profiler;

        spdlog::info("[login] background login job started");
        {
            LoginProfiler::Scope scope(profiler, "ensure_cluster");
            if (!localCluster.ensureRunning())
            {
                result.statusMessage = "Local servers are not running.";
                result.profileReport = profiler.summary();
                spdlog::error("[login] {}", result.statusMessage);
                return result;
            }
        }

        LoginSession handoffSession;
        if (!performLoginAndHandoff(profiler, handoffSession, result.statusMessage))
        {
            result.profileReport = profiler.summary();
            spdlog::error("[login] handoff failed: {}", result.statusMessage);
            return result;
        }

        result.ok = true;
        result.session = handoffSession;
        result.statusMessage = "Connected.";
        result.profileReport = profiler.summary();
        spdlog::info("[login] background login job completed");
        return result;
    }

    void applyInventorySnapshot(const tbeq::net::InventorySnapshotPayload& snapshot)
    {
        hudGold = snapshot.gold;
        inventoryWindow.applySnapshot(snapshot);
        std::string equippedWeapon;
        for (const auto& entry : snapshot.equipment)
        {
            if (entry.slot == "weapon")
            {
                equippedWeapon = entry.itemId;
                break;
            }
        }
        if (entityRenderer != nullptr)
        {
            entityRenderer->setLocalPlayerEquippedWeapon(equippedWeapon);
        }
    }

    void wireZoneClientCallbacks()
    {
        if (zoneClient == nullptr)
        {
            return;
        }

        zoneClient->setChatCallback([this](const tbeq::net::ChatDeliverPayload& deliver)
        {
            if (deliver.channel == static_cast<uint8_t>(tbeq::ChatChannel::System))
            {
                chatLines.push_back("[System] " + deliver.text);
            }
            else
            {
                chatLines.push_back("[Say] " + deliver.senderName + ": " + deliver.text);
            }
        });
        zoneClient->setCombatStartCallback([this](const tbeq::net::CombatStartPayload& start)
        {
            combatWindow.applyStart(start);
            for (const auto& participant : start.participants)
            {
                if (participant.isPlayerControlled)
                {
                    combatWindow.setPlayerCombatSlot(participant.combatSlot);
                    hudHp = participant.hp;
                    hudMaxHp = participant.maxHp;
                    hudMana = participant.mana;
                    hudMaxMana = participant.maxMana;
                    break;
                }
            }
            combatVisible = true;
            chatLines.push_back("[System] Combat started!");
        });
        zoneClient->setCombatUpdateCallback([this](const tbeq::net::CombatUpdatePayload& update)
        {
            combatWindow.applyUpdate(update);
            for (const auto& participant : update.participants)
            {
                if (participant.combatSlot == combatWindow.playerCombatSlot())
                {
                    hudHp = participant.hp;
                    hudMaxHp = participant.maxHp;
                    hudMana = participant.mana;
                    hudMaxMana = participant.maxMana;
                }
            }
        });
        zoneClient->setCombatEventCallback([this](const tbeq::net::CombatEventPayload& event)
        {
            combatWindow.applyEvent(event);
        });
        zoneClient->setCombatEndCallback([this](const tbeq::net::CombatEndPayload& end)
        {
            combatWindow.applyEnd(end);
            combatVisible = false;
            chatLines.push_back("[System] " + end.message);
        });
        zoneClient->setVitalsCallback([this](const tbeq::net::CharacterVitalsPayload& vitals)
        {
            combatWindow.applyVitals(vitals);
            hudHp = vitals.hp;
            hudMaxHp = vitals.maxHp;
            hudMana = vitals.mana;
            hudMaxMana = vitals.maxMana;
        });
        zoneClient->setSkillGainCallback([this](const tbeq::net::SkillGainPayload& gain)
        {
            combatWindow.applySkillGain(gain);
            chatLines.push_back("[System] " + gain.message);
        });
        zoneClient->setInventorySnapshotCallback([this](const tbeq::net::InventorySnapshotPayload& snapshot)
        {
            applyInventorySnapshot(snapshot);
        });
        zoneClient->setMerchantOpenCallback([this](const tbeq::net::MerchantOpenPayload& open)
        {
            merchantWindow.applyOpen(open);
            merchantVisible = true;
            merchantWindowPanel.state().visible = true;
            layoutManager.markDirty();
            chatLines.push_back("[System] Trading with " + open.npcName);
        });
    }

    void interactWithNpc(uint32_t npcEntityId)
    {
        if (zoneClient == nullptr || npcEntityId == 0)
        {
            return;
        }

        zoneClient->interactNpc(npcEntityId);
    }

    void tryInteractNearbyNpc()
    {
        if (zoneClient == nullptr || entityRenderer == nullptr)
        {
            return;
        }

        const auto npcEntityId = entityRenderer->nearestNpcEntity(
            zoneClient->playerTileX(),
            zoneClient->playerTileY(),
            3);
        if (npcEntityId.has_value())
        {
            if (const auto plate = entityRenderer->nearestNpcNameplate(
                    zoneClient->playerTileX(),
                    zoneClient->playerTileY(),
                    3))
            {
                chatLines.push_back("[System] Interacting with " + plate->name + " (" + plate->role + ").");
            }
            if (!zoneClient->interactNpc(*npcEntityId))
            {
                chatLines.push_back("[System] Could not open merchant. Move closer or try again.");
            }
        }
        else
        {
            chatLines.push_back("[System] No NPC nearby. Walk next to a gold-outlined merchant and press N.");
        }
    }

    bool completeEnterZone(const LoginSession& loginSession)
    {
        LoginProfiler profiler;
        LoginProfiler::Scope scope(profiler, "enter_zone_main_thread");

        zoneClient = std::make_unique<tbeq::client::ZoneClient>(io);
        if (!zoneClient->connect(loginSession.zoneConnect.zoneHost, loginSession.zoneConnect.zonePort))
        {
            statusMessage = "Failed to connect to zone server";
            profiler.logSummary("enter_zone_failed");
            return false;
        }

        wireZoneClientCallbacks();

        if (!zoneClient->sessionResume(
                loginSession.characterId,
                loginSession.sessionTokenHash,
                zoneSnapshot,
                true))
        {
            statusMessage = "Session resume failed";
            profiler.logSummary("enter_zone_failed");
            return false;
        }

        const auto* style = styleCatalog.find(zoneSnapshot.tileStyle);
        tilemapRenderer->setStyleCatalog(style);
        tilemapRenderer->setTileDefs(&tileDefs);
        tilemapRenderer->setZoneSnapshot(zoneSnapshot);

        entityRenderer->clear();
        entityRenderer->setStyleCatalog(style);
        entityRenderer->setSpriteCatalog(&entitySpriteCatalog);
        entityRenderer->setItemCatalog(&itemCatalog);
        entityRenderer->setLocalPlayer(
            zoneClient->playerEntityId(),
            loginSession.characterName.empty() ? characterNameBuffer : loginSession.characterName,
            loginSession.raceId,
            loginSession.classId,
            zoneClient->playerTileX(),
            zoneClient->playerTileY());
        while (auto snapshot = zoneClient->pollEntitySnapshot())
        {
            entityRenderer->applySnapshot(*snapshot);
        }

        applyInventorySnapshot(zoneClient->inventorySnapshot());

        cameraTileX = std::max(0, zoneClient->playerTileX() - 10);
        cameraTileY = std::max(0, zoneClient->playerTileY() - 6);
        state = ClientState::InZone;
        statusMessage = "In zone: " + zoneSnapshot.zoneName;
        chatLines.push_back("[System] Entered " + zoneSnapshot.zoneName + ". Gold figures are merchants (N to trade).");
        chatLines.push_back("[System] North portal at tile (32, 8): walk there and press P to leave the city.");
        profiler.logSummary("enter_zone");
        return true;
    }

    void pollLoginJob()
    {
        if (!loginInProgress || !loginFuture.valid())
        {
            return;
        }

        if (loginFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            return;
        }

        loginInProgress = false;
        LoginJobResult result = loginFuture.get();
        spdlog::info("[login] profile:\n{}", result.profileReport);

        if (!result.ok)
        {
            statusMessage = result.statusMessage.empty() ? "Login failed" : result.statusMessage;
            return;
        }

        session = result.session;
        if (!completeEnterZone(result.session))
        {
            return;
        }

        statusMessage = result.statusMessage;
    }

    void handleEvent(const SDL_Event& event)
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            running = false;
        }
        else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            width = event.window.data1;
            height = event.window.data2;
        }
#if TBEQ_ENABLE_DEBUG_MENU
        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)
        {
            debugWindow.state().visible = !debugWindow.state().visible;
            layoutManager.markDirty();
        }
#endif
        else if (state == ClientState::InZone && event.type == SDL_KEYDOWN && zoneClient != nullptr && !combatWindow.isActive())
        {
            if (event.key.keysym.sym == SDLK_i)
            {
                inventoryVisible = !inventoryVisible;
                inventoryWindowPanel.state().visible = inventoryVisible;
                layoutManager.markDirty();
                return;
            }
            if (event.key.keysym.sym == SDLK_n)
            {
                tryInteractNearbyNpc();
                return;
            }

            int32_t targetX = zoneClient->playerTileX();
            int32_t targetY = zoneClient->playerTileY();
            if (targetX == 0 && targetY == 0)
            {
                targetX = 32;
                targetY = 32;
            }

            if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP)
            {
                --targetY;
            }
            else if (event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_DOWN)
            {
                ++targetY;
            }
            else if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT)
            {
                --targetX;
            }
            else if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT)
            {
                ++targetX;
            }
            else if (event.key.keysym.sym == SDLK_p)
            {
                tbeq::net::ZoneConnectInfoPayload transfer;
                if (zoneClient->usePortal(transfer))
                {
                    session.zoneConnect = transfer;
                    zoneClient->close();
                    zoneClient = std::make_unique<tbeq::client::ZoneClient>(io);
                    zoneClient->connect(transfer.zoneHost, transfer.zonePort);
                    wireZoneClientCallbacks();
                    if (zoneClient->sessionResume(session.characterId, session.sessionTokenHash, zoneSnapshot))
                    {
                        const auto* style = styleCatalog.find(zoneSnapshot.tileStyle);
                        tilemapRenderer->setStyleCatalog(style);
                        tilemapRenderer->setZoneSnapshot(zoneSnapshot);
                        entityRenderer->clear();
                        entityRenderer->setStyleCatalog(style);
                        entityRenderer->setSpriteCatalog(&entitySpriteCatalog);
                        entityRenderer->setItemCatalog(&itemCatalog);
                        entityRenderer->setLocalPlayer(
                            zoneClient->playerEntityId(),
                            session.characterName.empty() ? characterNameBuffer : session.characterName,
                            session.raceId,
                            session.classId,
                            zoneClient->playerTileX(),
                            zoneClient->playerTileY());
                        while (auto snapshot = zoneClient->pollEntitySnapshot())
                        {
                            entityRenderer->applySnapshot(*snapshot);
                        }
                        applyInventorySnapshot(zoneClient->inventorySnapshot());
                        cameraTileX = std::max(0, zoneClient->playerTileX() - 10);
                        cameraTileY = std::max(0, zoneClient->playerTileY() - 6);
                        chatLines.push_back("[System] Transferred to " + zoneSnapshot.zoneName);
                    }
                }
                return;
            }
            else
            {
                return;
            }

            tbeq::net::MoveResultPayload moveResult;
            if (zoneClient->moveTo(targetX, targetY, moveResult) && moveResult.ok)
            {
                entityRenderer->updateLocalPlayerPosition(moveResult.tileX, moveResult.tileY);
                cameraTileX = std::max(0, moveResult.tileX - 10);
                cameraTileY = std::max(0, moveResult.tileY - 6);
            }
        }
        else if (state == ClientState::InZone && event.type == SDL_MOUSEBUTTONDOWN
            && event.button.button == SDL_BUTTON_LEFT
            && zoneClient != nullptr
            && entityRenderer != nullptr
            && !combatWindow.isActive()
            && !ImGui::GetIO().WantCaptureMouse)
        {
            const int tileSize = tbeq::render::TileGenerator::kTileSize;
            const int mouseX = event.button.x;
            const int mouseY = event.button.y;
            const int32_t tileX = cameraTileX + mouseX / tileSize;
            const int32_t tileY = cameraTileY + mouseY / tileSize;
            const auto npcEntityId = entityRenderer->nearestNpcEntity(tileX, tileY, 1);
            if (npcEntityId.has_value())
            {
                if (!zoneClient->interactNpc(*npcEntityId))
                {
                    chatLines.push_back("[System] Could not open merchant. Move closer or try again.");
                }
            }
        }
    }

    void renderWorld()
    {
        if (state != ClientState::InZone || tilemapRenderer == nullptr || entityRenderer == nullptr)
        {
            return;
        }

        const uint64_t tickMs = SDL_GetTicks64();
        tilemapRenderer->updateAnimation(tickMs);

        const int viewTilesWide = width / tbeq::render::TileGenerator::kTileSize + 2;
        const int viewTilesHigh = height / tbeq::render::TileGenerator::kTileSize + 2;

        if (zoneClient != nullptr && entityRenderer != nullptr)
        {
            cameraTileX = std::max(0, zoneClient->playerTileX() - 10);
            cameraTileY = std::max(0, zoneClient->playerTileY() - 6);
        }

        tilemapRenderer->render(cameraTileX, cameraTileY, viewTilesWide, viewTilesHigh);

        if (zoneClient != nullptr)
        {
            zoneClient->pollGameplayPackets();

            tbeq::net::SubmitActionPayload action;
            if (combatWindow.consumePendingAction(action))
            {
                tbeq::net::SubmitActionResultPayload result;
                zoneClient->submitAction(action, result);
            }

            if (auto snapshot = zoneClient->pollEntitySnapshot())
            {
                entityRenderer->applySnapshot(*snapshot);
            }

            entityRenderer->render(cameraTileX, cameraTileY, viewTilesWide, viewTilesHigh);
        }
    }

    void renderWorldNameplates()
    {
        if (state != ClientState::InZone || entityRenderer == nullptr)
        {
            return;
        }

        const int viewTilesWide = width / tbeq::render::TileGenerator::kTileSize + 2;
        const int viewTilesHigh = height / tbeq::render::TileGenerator::kTileSize + 2;
        drawWorldNameplates(
            entityRenderer->visibleNameplates(cameraTileX, cameraTileY, viewTilesWide, viewTilesHigh),
            cameraTileX,
            cameraTileY);
    }

    void renderLoginPanel()
    {
        ImGui::SetNextWindowPos(ImVec2(width * 0.5f, height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 260.0f), ImGuiCond_Always);
        ImGui::Begin("TurnBasedEQ Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        ImGui::InputText("Username", usernameBuffer, sizeof(usernameBuffer));
        ImGui::InputText("Password", passwordBuffer, sizeof(passwordBuffer), ImGuiInputTextFlags_Password);
        ImGui::InputText("Character Name", characterNameBuffer, sizeof(characterNameBuffer));
        ImGui::TextWrapped("%s", statusMessage.c_str());
        const bool canStartLogin = !loginInProgress;
        if (!canStartLogin)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Enter World"))
        {
            loginInProgress = true;
            statusMessage = "Logging in... (see log for profile)";
            spdlog::info("[login] Enter World clicked");
            loginFuture = std::async(std::launch::async, [this]()
            {
                return runLoginJob();
            });
        }
        if (!canStartLogin)
        {
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

#if TBEQ_ENABLE_DEBUG_MENU
    void renderDebugMenu()
    {
        const bool drawContent = debugWindow.begin(width, height);
        if (drawContent)
        {
            debugMenu.update(
                zoneClient.get(),
                [this](const std::string& line)
                {
                    chatLines.push_back(line);
                });
        }
        if (debugWindow.syncFromImGui())
        {
            layoutManager.markDirty();
        }
        debugWindow.end();
    }
#endif

    void renderShellWindows()
    {
        if (state == ClientState::Login)
        {
            renderLoginPanel();
#if TBEQ_ENABLE_DEBUG_MENU
            renderDebugMenu();
#endif
            return;
        }

        if (hudWindow.begin(width, height))
        {
            ImGui::Text("Zone: %s", zoneSnapshot.zoneName.c_str());
            ImGui::Text("Tile style: %s", zoneSnapshot.tileStyle.c_str());
            ImGui::Text("Position: (%d, %d)", zoneClient != nullptr ? zoneClient->playerTileX() : 0, zoneClient != nullptr ? zoneClient->playerTileY() : 0);
            ImGui::ProgressBar(
                hudMaxHp > 0 ? static_cast<float>(hudHp) / static_cast<float>(hudMaxHp) : 0.0f,
                ImVec2(-1.0f, 0.0f),
                (std::to_string(hudHp) + " / " + std::to_string(hudMaxHp)).c_str());
            ImGui::Text("Mana: %u / %u", hudMana, hudMaxMana);
            ImGui::Text("Gold: %u", hudGold);
            ImGui::TextUnformatted("WASD move | I inventory | N interact | P portal | F1 debug");
        }
        if (hudWindow.syncFromImGui())
        {
            layoutManager.markDirty();
        }
        hudWindow.end();

        if (minimapWindow.begin(width, height))
        {
            const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            const float scale = std::min(
                canvasSize.x / static_cast<float>(std::max(zoneSnapshot.width, 1)),
                canvasSize.y / static_cast<float>(std::max(zoneSnapshot.height, 1)));
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            for (int32_t y = 0; y < zoneSnapshot.height; ++y)
            {
                for (int32_t x = 0; x < zoneSnapshot.width; ++x)
                {
                    const SDL_Color color = tilemapRenderer->minimapColorAt(x, y);
                    const ImVec2 p0(origin.x + x * scale, origin.y + y * scale);
                    const ImVec2 p1(p0.x + scale, p0.y + scale);
                    drawList->AddRectFilled(
                        p0,
                        p1,
                        IM_COL32(color.r, color.g, color.b, color.a));
                }
            }

            if (entityRenderer != nullptr)
            {
                const float dotSize = std::max(2.0f, scale * 2.0f);
                for (const auto& dot : entityRenderer->minimapDots())
                {
                    ImU32 dotColor = IM_COL32(220, 220, 220, 255);
                    if (dot.isLocalPlayer)
                    {
                        dotColor = IM_COL32(255, 255, 80, 255);
                    }
                    else if (dot.entityType != 0)
                    {
                        dotColor = IM_COL32(255, 210, 80, 255);
                    }

                    const ImVec2 center(
                        origin.x + (dot.tileX + 0.5f) * scale,
                        origin.y + (dot.tileY + 0.5f) * scale);
                    drawList->AddRectFilled(
                        ImVec2(center.x - dotSize * 0.5f, center.y - dotSize * 0.5f),
                        ImVec2(center.x + dotSize * 0.5f, center.y + dotSize * 0.5f),
                        dotColor);
                }
            }
        }
        if (minimapWindow.syncFromImGui())
        {
            layoutManager.markDirty();
        }
        minimapWindow.end();

        if (chatWindow.begin(width, height))
        {
            ImGui::BeginChild("ChatScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
            for (const auto& line : chatLines)
            {
                ImGui::TextUnformatted(line.c_str());
            }
            ImGui::EndChild();

            static char inputBuffer[256] = {};
            if (ImGui::InputText("##chat", inputBuffer, sizeof(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if (zoneClient != nullptr && inputBuffer[0] != '\0')
                {
                    zoneClient->sendSayChat(inputBuffer);
                    chatLines.push_back(std::string("[Say] You: ") + inputBuffer);
                    inputBuffer[0] = '\0';
                }
            }
        }
        if (chatWindow.syncFromImGui())
        {
            layoutManager.markDirty();
        }
        chatWindow.end();

        combatWindow.draw(combatWindowPanel, combatVisible, width, height);
        if (combatWindowPanel.syncFromImGui())
        {
            layoutManager.markDirty();
        }

        inventoryWindow.draw(
            inventoryWindowPanel,
            inventoryVisible,
            zoneClient.get(),
            renderer,
            width,
            height,
            [this](const std::string& line)
            {
                chatLines.push_back(line);
            });
        if (inventoryWindowPanel.syncFromImGui())
        {
            layoutManager.markDirty();
        }

        merchantWindow.draw(
            merchantWindowPanel,
            merchantVisible,
            zoneClient.get(),
            renderer,
            zoneClient != nullptr ? zoneClient->inventorySnapshot() : tbeq::net::InventorySnapshotPayload{},
            width,
            height,
            [this](const std::string& line)
            {
                chatLines.push_back(line);
            });
        if (merchantWindowPanel.syncFromImGui())
        {
            layoutManager.markDirty();
        }

#if TBEQ_ENABLE_DEBUG_MENU
        renderDebugMenu();
#endif
    }

    void frame()
    {
        pollLoginJob();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        renderWorld();

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        tbeq::ui::constrainActiveImGuiWindowDrag(width, height);

        renderWorldNameplates();
        renderShellWindows();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
};

} // namespace

int main(int, char**)
{
    tbeq::initLogging("client");
    spdlog::info("Press F1 to toggle debug menu (Debug builds)");

    ClientApp app;
    if (!app.prepareEnvironment())
    {
        return EXIT_FAILURE;
    }

    if (!app.init())
    {
        app.shutdown();
        return EXIT_FAILURE;
    }

    while (app.running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            app.handleEvent(event);
        }
        app.frame();
    }

    app.shutdown();
    return EXIT_SUCCESS;
}
