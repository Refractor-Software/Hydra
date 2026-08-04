add_library(com.refractor.core.foundation STATIC)

target_include_directories(com.refractor.core.foundation PUBLIC include)
target_include_directories(com.refractor.core.foundation PRIVATE src)

file(GLOB_RECURSE foundation_headers CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE foundation_sources CONFIGURE_DEPENDS "src/*.h" "src/*.c")

target_sources(com.refractor.core.foundation PUBLIC ${foundation_headers})
target_sources(com.refractor.core.foundation PRIVATE ${foundation_sources})

add_subdirectory(test)
