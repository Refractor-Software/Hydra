cmake_minimum_required(VERSION 3.29)

project(Hydra LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)

set(HYDRA_TARGET_ISA "AVX" CACHE STRING "Minimum required CPU instruction set (AVX or AVX2)")
set_property(CACHE HYDRA_TARGET_ISA PROPERTY STRINGS AVX AVX2)

if (NOT HYDRA_TARGET_ISA STREQUAL "AVX" AND NOT HYDRA_TARGET_ISA STREQUAL "AVX2")
    message(FATAL_ERROR "HYDRA_TARGET_ISA must be AVX or AVX2 (got: ${HYDRA_TARGET_ISA})")
endif ()

if (MSVC)
    add_compile_options(/arch:${HYDRA_TARGET_ISA})
endif ()

if (HYDRA_TARGET_ISA STREQUAL "AVX2")
    add_compile_definitions(HYDRA_TARGET_ISA_AVX2=1)
else ()
    add_compile_definitions(HYDRA_TARGET_ISA_AVX2=0)
endif ()

add_subdirectory(kernel)

add_subdirectory(core)
