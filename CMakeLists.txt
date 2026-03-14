cmake_minimum_required(VERSION 4.1)
project(Hydra)

include(CMake/ModuleTools.cmake)
include(CMake/VisualStudioSolution.cmake)

add_subdirectory(vendor)
add_subdirectory(modules)

add_executable(Hydra main.cpp)

target_link_libraries(Hydra PRIVATE hydra_foundation)
