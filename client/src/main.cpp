#include <cstdlib>
#include <filesystem>
#include <string>

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <spdlog/spdlog.h>

#include "tbeq/core/Log.hpp"
#include "ui/GameWindow.hpp"
#include "ui/WindowLayoutManager.hpp"

#if TBEQ_ENABLE_DEBUG_MENU
#include "ui/debug/DebugMenu.hpp"
#include "ui/debug/LogViewer.hpp"
#endif

namespace
{

struct ClientApp
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 1280;
    int height = 720;
    bool running = true;

    tbeq::ui::WindowLayoutManager layoutManager{"config/ui_layout.json"};
    tbeq::ui::GameWindow hudWindow{"hud", "HUD", 200.0f, 60.0f};
    tbeq::ui::GameWindow chatWindow{"chat", "Chat", 320.0f, 180.0f};

#if TBEQ_ENABLE_DEBUG_MENU
    tbeq::ui::GameWindow debugWindow{"debug_menu", "Debug Menu", 400.0f, 300.0f};
    tbeq::ui::LogViewer logViewer{tbeq::getRingBufferSink()};
    tbeq::ui::DebugMenu debugMenu{logViewer};
    bool debugVisible = false;
#endif

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
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        layoutManager.registerWindow(hudWindow);
        layoutManager.registerWindow(chatWindow);
#if TBEQ_ENABLE_DEBUG_MENU
        layoutManager.registerWindow(debugWindow);
#endif
        layoutManager.load();

        spdlog::info("TurnBasedEQ client started");
        return true;
    }

    void shutdown()
    {
        layoutManager.save();

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
            debugVisible = !debugVisible;
            debugWindow.state().visible = debugVisible;
            layoutManager.markDirty();
        }
#endif
    }

    void renderShellWindows()
    {
        if (hudWindow.begin(width, height))
        {
            ImGui::Text("HP: 100 / 100");
            ImGui::Text("Mana: 50 / 50");
            ImGui::Text("Zone: starter (Phase 0 shell)");
            hudWindow.end();
            layoutManager.markDirty();
        }

        if (chatWindow.begin(width, height))
        {
            ImGui::BeginChild("ChatScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
            ImGui::TextUnformatted("[System] Welcome to TurnBasedEQ.");
            ImGui::TextUnformatted("[Say] Drag and resize this window to test layout persistence.");
            ImGui::EndChild();

            static char inputBuffer[256] = {};
            ImGui::InputText("##chat", inputBuffer, sizeof(inputBuffer));
            chatWindow.end();
            layoutManager.markDirty();
        }

#if TBEQ_ENABLE_DEBUG_MENU
        if (debugWindow.begin(width, height))
        {
            debugMenu.update(debugVisible);
            debugWindow.end();
            layoutManager.markDirty();
        }
#endif
    }

    void frame()
    {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        renderShellWindows();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderClear(renderer);
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
    if (!app.init())
    {
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
