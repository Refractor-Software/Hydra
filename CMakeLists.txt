cmake_minimum_required(VERSION 3.29)

project(Hydra LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(RE_TARGET_ISA "AVX" CACHE STRING "Minimum required CPU instruction set (AVX or AVX2)")
set_property(CACHE RE_TARGET_ISA PROPERTY STRINGS AVX AVX2)

if (NOT RE_TARGET_ISA STREQUAL "AVX" AND NOT RE_TARGET_ISA STREQUAL "AVX2")
    message(FATAL_ERROR "RE_TARGET_ISA must be AVX or AVX2 (got: ${RE_TARGET_ISA})")
endif ()

if (MSVC)
    add_compile_options(/arch:${RE_TARGET_ISA})
endif ()

if (RE_TARGET_ISA STREQUAL "AVX2")
    add_compile_definitions(RE_TARGET_ISA_AVX2=1)
else ()
    add_compile_definitions(RE_TARGET_ISA_AVX2=0)
endif ()

#[[
    RE_BUILD is an ordered scale, not a boolean - see RE/Foundation/FoundationBuild.h, which
    defines the level names this expands to. The three levels map onto the three presets in
    CMakePresets.json.
#]]
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(RE_BUILD=RE_BUILD_SHIPPING)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(RE_BUILD=RE_BUILD_DEVELOPMENT)
else ()
    add_compile_definitions(RE_BUILD=RE_BUILD_DEBUG)
endif ()

add_subdirectory(kernel)

add_subdirectory(core)
