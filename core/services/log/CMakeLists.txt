add_library(log STATIC)

target_include_directories(log PUBLIC include)
target_include_directories(log PRIVATE src)

file(GLOB_RECURSE log_headers "include/*.h")
file(GLOB_RECURSE log_sources "src/*.h" "src/*.c")

target_sources(log PUBLIC ${log_headers})
target_sources(log PRIVATE ${log_sources})

target_link_libraries(log PUBLIC foundation)
