add_executable(tbeq_client
    src/main.cpp
    net/LocalClusterLauncher.cpp
    net/LoginProfiler.cpp
    net/ZoneClient.cpp
    render/procedural/TileGenerator.cpp
    render/procedural/SpriteGenerator.cpp
    render/procedural/ItemIconGenerator.cpp
    render/SpriteAtlas.cpp
    render/TextureCache.cpp
    render/SpriteRenderer.cpp
    render/TilemapRenderer.cpp
    render/EntityRenderer.cpp
    ui/GameWindow.cpp
    ui/WindowLayoutManager.cpp
    ui/combat/CombatWindow.cpp
    ui/inventory/InventoryWindow.cpp
    ui/merchant/MerchantWindow.cpp
    ui/look/LookWindow.cpp
    ui/dialog/NpcDialogWindow.cpp
    ui/skills/SkillsWindow.cpp
    ui/MechanicToolbar.cpp
    ui/debug/DebugMenu.cpp
    ui/debug/LogViewer.cpp
    ui/debug/UnitTestRunner.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui-src/backends/imgui_impl_sdl2.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui-src/backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(tbeq_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/debug
        ${CMAKE_CURRENT_SOURCE_DIR}/net
        ${CMAKE_CURRENT_SOURCE_DIR}/render
        ${CMAKE_SOURCE_DIR}/third_party/imgui-src/backends
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(tbeq_client
    PRIVATE
        tbeq_shared
        SDL2::SDL2
        SDL2::SDL2main
        imgui::imgui
)

target_compile_definitions(tbeq_client PRIVATE TBEQ_REPO_ROOT="${CMAKE_SOURCE_DIR}")

if(TBEQ_ENABLE_DEBUG_MENU)
    target_compile_definitions(tbeq_client PRIVATE
        TBEQ_ENABLE_DEBUG_MENU=1
    )
else()
    target_compile_definitions(tbeq_client PRIVATE TBEQ_ENABLE_DEBUG_MENU=0)
endif()

if(TARGET SDL2::SDL2 AND TARGET SDL2::SDL2-shared)
    add_custom_command(TARGET tbeq_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2-shared>"
            "$<TARGET_FILE_DIR:tbeq_client>"
        COMMENT "Copying SDL2 runtime library next to tbeq_client")
elseif(TARGET SDL2::SDL2)
    add_custom_command(TARGET tbeq_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>"
            "$<TARGET_FILE_DIR:tbeq_client>"
        COMMENT "Copying SDL2 runtime library next to tbeq_client")
endif()
