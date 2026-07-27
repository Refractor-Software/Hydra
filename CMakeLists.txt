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
    How the heap gets from a bare pointer to its size class - the decision that defines the
    allocator's character, so it is a build option rather than something buried in a header.

    SIDE_TABLE     two dependent loads per free, no large reservation, no per-class ceiling.
    ADDRESS_SLICE  no lookup at all, but reserves tens of GB of address space and imposes a hard
                   per-class cap.

    Compile time, not run time: a runtime switch would put an indirect branch on the hottest path
    in the allocator, which is exactly the cost ADDRESS_SLICE exists to avoid. Both are always
    compiled by the test target so neither rots.
#]]
set(RE_HEAP_MAP_STRATEGY "SIDE_TABLE" CACHE STRING "Heap pointer-to-class strategy (SIDE_TABLE or ADDRESS_SLICE)")
set_property(CACHE RE_HEAP_MAP_STRATEGY PROPERTY STRINGS SIDE_TABLE ADDRESS_SLICE)

if (RE_HEAP_MAP_STRATEGY STREQUAL "SIDE_TABLE")
    add_compile_definitions(RE_HEAP_MAP_STRATEGY=RE_HEAP_MAP_STRATEGY_SIDE_TABLE)
elseif (RE_HEAP_MAP_STRATEGY STREQUAL "ADDRESS_SLICE")
    add_compile_definitions(RE_HEAP_MAP_STRATEGY=RE_HEAP_MAP_STRATEGY_ADDRESS_SLICE)
else ()
    message(FATAL_ERROR "RE_HEAP_MAP_STRATEGY must be SIDE_TABLE or ADDRESS_SLICE (got: ${RE_HEAP_MAP_STRATEGY})")
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

enable_testing()

add_subdirectory(kernel)

add_subdirectory(core)
