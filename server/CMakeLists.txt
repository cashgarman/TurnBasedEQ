add_library(tbeq_server_common
    common/debug/DebugCommandHandler.cpp
    common/net/TcpConnection.cpp
)

target_include_directories(tbeq_server_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_link_libraries(tbeq_server_common
    PUBLIC
        tbeq_shared
)

add_executable(tbeq_world_login
    world_login/main.cpp
)

target_link_libraries(tbeq_world_login
    PRIVATE
        tbeq_server_common
)

add_executable(tbeq_zone_server
    zone/main.cpp
)

target_link_libraries(tbeq_zone_server
    PRIVATE
        tbeq_server_common
)
