cmake_minimum_required(VERSION 3.29)

project(Hydra LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)

add_subdirectory(kernel)

add_subdirectory(core)
