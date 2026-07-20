add_library(foundation STATIC)

target_include_directories(foundation PUBLIC include)
target_include_directories(foundation PRIVATE src)

file(GLOB_RECURSE foundation_headers "include/*.h")
file(GLOB_RECURSE foundation_sources "src/*.h" "src/*.c")

target_sources(foundation PUBLIC ${foundation_headers})
target_sources(foundation PRIVATE ${foundation_sources})
