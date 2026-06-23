cmake_minimum_required(VERSION 3.24)

add_library(tbeq_test_helpers
    helpers/TestCluster.cpp
    helpers/HeadlessClient.cpp
    helpers/TempDatabase.cpp
)

target_include_directories(tbeq_test_helpers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
        ${CMAKE_SOURCE_DIR}/server/common
        ${CMAKE_SOURCE_DIR}/client/ui
)

target_link_libraries(tbeq_test_helpers
    PUBLIC
        tbeq_shared
        tbeq_server_common
        tbeq_asio
)

target_compile_definitions(tbeq_test_helpers
    PUBLIC
        TBEQ_REPO_ROOT="${CMAKE_SOURCE_DIR}"
        ASIO_STANDALONE
)

add_executable(tbeq_tests_unit
    unit/net/packet_roundtrip_test.cpp
    unit/server/debug_command_test.cpp
    unit/ui/ui_layout_test.cpp
    unit/auth/auth_test.cpp
    unit/worldgen/world_validator_test.cpp
    unit/world/collision_test.cpp
    unit/render/tile_generator_test.cpp
    unit/render/sprite_generator_test.cpp
    unit/shared/combat_initiative_test.cpp
    unit/shared/combat_hit_miss_test.cpp
    unit/shared/combat_flee_test.cpp
    unit/shared/spell_combat_test.cpp
    unit/shared/equip_rules_test.cpp
    unit/shared/merchant_pricing_test.cpp
    unit/ai/class_combat_brain_test.cpp
)

target_link_libraries(tbeq_tests_unit
    PRIVATE
        tbeq_test_helpers
        tbeq_shared
        tbeq_server_common
        Catch2::Catch2WithMain
        imgui::imgui
)

target_sources(tbeq_tests_unit PRIVATE
    ${CMAKE_SOURCE_DIR}/client/ui/GameWindow.cpp
    ${CMAKE_SOURCE_DIR}/client/ui/WindowLayoutManager.cpp
)

target_include_directories(tbeq_tests_unit
    PRIVATE
        ${CMAKE_SOURCE_DIR}/client/ui
        ${CMAKE_SOURCE_DIR}/client/render
        ${CMAKE_SOURCE_DIR}/server/common
)

target_sources(tbeq_tests_unit PRIVATE
    ${CMAKE_SOURCE_DIR}/client/render/procedural/TileGenerator.cpp
    ${CMAKE_SOURCE_DIR}/client/render/procedural/SpriteGenerator.cpp
)

add_executable(tbeq_tests_integration
    integration/cluster_registers_test.cpp
    integration/login_and_handoff_test.cpp
    integration/two_players_and_portal_test.cpp
    integration/combat_encounter_test.cpp
    integration/ai_party_combat_test.cpp
    integration/merchant_buy_test.cpp
)

target_link_libraries(tbeq_tests_integration
    PRIVATE
        tbeq_test_helpers
        tbeq_shared
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(tbeq_tests_unit)
catch_discover_tests(tbeq_tests_integration PROPERTIES RESOURCE_LOCK integration_servers)
