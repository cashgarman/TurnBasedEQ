cmake_minimum_required(VERSION 3.24)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(TbeqCompilerCache)

project(TurnBasedEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        add_compile_options(/FS /MP)
    endif()
endif()

option(TBEQ_BUILD_TESTS "Build test suite" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(TBEQ_ENABLE_DEBUG_MENU_DEFAULT ON)
else()
    set(TBEQ_ENABLE_DEBUG_MENU_DEFAULT OFF)
endif()
option(TBEQ_ENABLE_DEBUG_MENU "Enable client debug menu UI" ${TBEQ_ENABLE_DEBUG_MENU_DEFAULT})

include(FetchContent)

set(TBEQ_THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party/cache)

FetchContent_Declare(
    asio
    URL ${TBEQ_THIRD_PARTY_DIR}/chriskohlhoff-asio-asio-1-32-0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    nlohmann_json
    URL ${TBEQ_THIRD_PARTY_DIR}/json-3.12.0.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    Catch2
    URL ${TBEQ_THIRD_PARTY_DIR}/catchorg-Catch2-v3.15.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    SDL2
    URL ${TBEQ_THIRD_PARTY_DIR}/libsdl-org-SDL-release-2.32.10.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    fmt
    URL ${TBEQ_THIRD_PARTY_DIR}/fmtlib-fmt-12.2.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    spdlog
    URL ${TBEQ_THIRD_PARTY_DIR}/gabime-spdlog-v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    sqlite3
    URL ${TBEQ_THIRD_PARTY_DIR}/sqlite-autoconf-3530200.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(asio nlohmann_json Catch2 fmt spdlog SDL2)

FetchContent_GetProperties(sqlite3)
if(NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
endif()

set(TBEQ_SQLITE3_SOURCE_DIR ${sqlite3_SOURCE_DIR} CACHE INTERNAL "")

add_library(tbeq_asio INTERFACE)
target_include_directories(tbeq_asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(tbeq_asio INTERFACE ASIO_STANDALONE)

add_library(tbeq_sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(tbeq_sqlite3 PRIVATE ${sqlite3_SOURCE_DIR})
target_compile_definitions(tbeq_sqlite3 PUBLIC SQLITE_ENABLE_JSON1)

find_package(imgui CONFIG REQUIRED)

if(TBEQ_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tools)

if(TBEQ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
