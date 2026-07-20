add_library(application STATIC)

target_include_directories(application PUBLIC include)
target_include_directories(application PRIVATE src)

file(GLOB_RECURSE application_headers "include/*.h")
file(GLOB_RECURSE application_sources "src/*.h" "src/*.c")

target_sources(application PUBLIC ${application_headers})
target_sources(application PRIVATE ${application_sources})

target_link_libraries(application PUBLIC foundation input)
