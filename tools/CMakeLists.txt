add_executable(tbeq_data_validator
    data_validator/main.cpp
)

target_link_libraries(tbeq_data_validator
    PRIVATE
        nlohmann_json::nlohmann_json
)

add_executable(tbeq_worldgen
    worldgen/main.cpp
)

target_link_libraries(tbeq_worldgen
    PRIVATE
        tbeq_shared
)

add_test(NAME content_validation
    COMMAND tbeq_data_validator ${CMAKE_SOURCE_DIR}/data
)

add_test(NAME worldgen_validation
    COMMAND tbeq_worldgen --seed 42 --validate-only --data-root ${CMAKE_SOURCE_DIR}/data
)
