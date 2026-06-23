add_library(tbeq_shared
    src/core/Log.cpp
    src/core/Config.cpp
    src/net/PacketSerializer.cpp
    src/social/ChatChannel.cpp
    src/npc/DialogTree.cpp
    src/npc/InteractionResolver.cpp
    src/skills/SkillResolver.cpp
    src/social/Invite.cpp
)

target_include_directories(tbeq_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tbeq_shared
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        tbeq_asio
)

target_compile_definitions(tbeq_shared
    PUBLIC
        ASIO_STANDALONE
)
