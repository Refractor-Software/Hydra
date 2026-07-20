add_library(input STATIC)

target_include_directories(input PUBLIC include)
target_include_directories(input PRIVATE src)

file(GLOB_RECURSE input_headers "include/*.h")
file(GLOB_RECURSE input_sources "src/*.h" "src/*.c")

target_sources(input PUBLIC ${input_headers})
target_sources(input PRIVATE ${input_sources})

target_link_libraries(input PUBLIC foundation)
