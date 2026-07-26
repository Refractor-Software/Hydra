add_library(com.refractor.core.services.input STATIC)

target_include_directories(com.refractor.core.services.input PUBLIC include)
target_include_directories(com.refractor.core.services.input PRIVATE src)

file(GLOB_RECURSE input_headers "include/*.h")
file(GLOB_RECURSE input_sources "src/*.h" "src/*.c")

target_sources(com.refractor.core.services.input PUBLIC ${input_headers})
target_sources(com.refractor.core.services.input PRIVATE ${input_sources})

target_link_libraries(com.refractor.core.services.input PUBLIC com.refractor.core.foundation)
