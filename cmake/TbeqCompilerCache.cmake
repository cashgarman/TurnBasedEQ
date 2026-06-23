# Must be included before project() so compiler launchers take effect.
option(TBEQ_USE_SCCACHE "Use sccache as a compiler launcher when available" ON)

if(TBEQ_USE_SCCACHE)
    find_program(TBEQ_SCCACHE_EXECUTABLE NAMES sccache)
    if(TBEQ_SCCACHE_EXECUTABLE)
        set(CMAKE_C_COMPILER_LAUNCHER "${TBEQ_SCCACHE_EXECUTABLE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${TBEQ_SCCACHE_EXECUTABLE}")
        message(STATUS "Compiler cache: sccache (${TBEQ_SCCACHE_EXECUTABLE})")
    elseif(TBEQ_USE_SCCACHE)
        message(STATUS "Compiler cache: sccache not found (install sccache for faster rebuilds)")
    endif()
endif()
