cmake_minimum_required(VERSION 4.1)
project(Hydra)

include(cmake/module_tools.cmake)

add_subdirectory(modules)

add_executable(Hydra main.cpp)

target_link_libraries(Hydra PRIVATE hydra_foundation)
