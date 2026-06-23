add_library(tbeq_shared
    src/core/Log.cpp
    src/core/Config.cpp
    src/core/CharacterState.cpp
    src/net/PacketSerializer.cpp
    src/persistence/Database.cpp
    src/auth/PasswordHash.cpp
    src/auth/CharacterValidation.cpp
    src/worldgen/WorldGenerator.cpp
    src/worldgen/WorldValidator.cpp
    src/worldgen/WorldBootstrap.cpp
    src/worldgen/WorldLayoutGraph.cpp
    src/worldgen/ZoneTypeCatalog.cpp
    src/worldgen/ZoneLayoutUtils.cpp
    src/worldgen/PortalPlacementResolver.cpp
    src/worldgen/ZoneGenerator.cpp
    src/world/TileDefCatalog.cpp
    src/world/ZoneGrid.cpp
    src/content/MobCatalog.cpp
    src/content/SpellCatalog.cpp
    src/content/AbilityCatalog.cpp
    src/content/ItemCatalog.cpp
    src/content/NpcCatalog.cpp
    src/content/SkillCatalog.cpp
    src/content/SkillCapCatalog.cpp
    src/items/ItemRules.cpp
    src/combat/CombatInstance.cpp
    src/combat/BossScriptCatalog.cpp
    src/ai/ClassCombatProfileCatalog.cpp
    src/ai/ClassCombatBrain.cpp
    src/ai/PartyMemberAI.cpp
    src/social/ChatChannel.cpp
    src/npc/DialogTree.cpp
    src/npc/InteractionResolver.cpp
    src/skills/SkillResolver.cpp
    src/skills/Progression.cpp
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
        tbeq_sqlite3
)

set_source_files_properties(src/persistence/Database.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${TBEQ_SQLITE3_SOURCE_DIR}"
)

target_compile_definitions(tbeq_shared
    PUBLIC
        ASIO_STANDALONE
)
