add_library(com.refractor.core.application STATIC)

target_include_directories(com.refractor.core.application PUBLIC include)
target_include_directories(com.refractor.core.application PRIVATE src)

file(GLOB_RECURSE application_headers CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE application_sources CONFIGURE_DEPENDS "src/*.h" "src/*.c")

target_sources(com.refractor.core.application PUBLIC ${application_headers})
target_sources(com.refractor.core.application PRIVATE ${application_sources})

target_link_libraries(com.refractor.core.application PUBLIC
    com.refractor.core.foundation
    com.refractor.core.services.input
    com.refractor.core.services.log)
