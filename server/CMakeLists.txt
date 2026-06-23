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
    world_login/WorldLoginServer.cpp
)

target_include_directories(tbeq_world_login
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/world_login
)

target_link_libraries(tbeq_world_login
    PRIVATE
        tbeq_server_common
)

add_executable(tbeq_zone_server
    zone/main.cpp
    zone/ZoneServer.cpp
)

target_include_directories(tbeq_zone_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/zone
)

target_link_libraries(tbeq_zone_server
    PRIVATE
        tbeq_server_common
)
