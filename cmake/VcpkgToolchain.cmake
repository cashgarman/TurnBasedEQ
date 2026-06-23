if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    set(_tbeq_vcpkg_root "$ENV{VCPKG_ROOT}")
else()
    set(_tbeq_vcpkg_root "C:/vcpkg")
endif()

set(_tbeq_vcpkg_toolchain "${_tbeq_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_tbeq_vcpkg_toolchain}")
    message(FATAL_ERROR
        "vcpkg toolchain not found at ${_tbeq_vcpkg_toolchain}. "
        "Set VCPKG_ROOT to your vcpkg clone.")
endif()

include("${_tbeq_vcpkg_toolchain}")
