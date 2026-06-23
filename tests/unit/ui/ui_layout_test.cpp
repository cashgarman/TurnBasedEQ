#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "GameWindow.hpp"
#include "WindowLayoutManager.hpp"

TEST_CASE("UI layout save/load round-trip", "[ui]")
{
    const auto tempPath = std::filesystem::temp_directory_path() / "tbeq_ui_layout_test.json";
    std::filesystem::remove(tempPath);

    tbeq::ui::WindowLayoutManager manager(tempPath);
    tbeq::ui::GameWindow chatWindow("chat", "Chat", 320.0f, 180.0f);
    chatWindow.state().x = 42.0f;
    chatWindow.state().y = 84.0f;
    chatWindow.state().width = 400.0f;
    chatWindow.state().height = 220.0f;
    chatWindow.state().visible = true;

    manager.registerWindow(chatWindow);
    manager.save();

    tbeq::ui::GameWindow loadedWindow("chat", "Chat", 320.0f, 180.0f);
    tbeq::ui::WindowLayoutManager loader(tempPath);
    loader.registerWindow(loadedWindow);
    loader.load();

    REQUIRE(loadedWindow.state().x == chatWindow.state().x);
    REQUIRE(loadedWindow.state().y == chatWindow.state().y);
    REQUIRE(loadedWindow.state().width == chatWindow.state().width);
    REQUIRE(loadedWindow.state().height == chatWindow.state().height);
    REQUIRE(loadedWindow.state().visible == chatWindow.state().visible);

    std::filesystem::remove(tempPath);
}

TEST_CASE("GameWindow clamp keeps half window visible", "[ui]")
{
    tbeq::ui::GameWindow window("hud", "HUD", 200.0f, 60.0f);
    window.state().x = -500.0f;
    window.state().y = -50.0f;
    window.state().width = 240.0f;
    window.state().height = 80.0f;

    window.clampToDisplay(1280, 720);
    REQUIRE(window.state().x >= -120.0f);
    REQUIRE(window.state().y >= 0.0f);
}
