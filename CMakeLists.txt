cmake_minimum_required(VERSION 4.1)
project(Hydra VERSION 0.1.0)

include(CMake/ModuleTools.cmake)
include(CMake/VisualStudioSolution.cmake)

add_subdirectory(ThirdParty)
add_subdirectory(Runtime)

include(CMake/HydraInstall.cmake)