add_executable(tbeq_data_validator
    data_validator/main.cpp
)

target_link_libraries(tbeq_data_validator
    PRIVATE
        nlohmann_json::nlohmann_json
)

add_test(NAME content_validation
    COMMAND tbeq_data_validator ${CMAKE_SOURCE_DIR}/data
)
