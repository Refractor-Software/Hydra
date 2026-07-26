add_library(com.refractor.core.services.log STATIC)

target_include_directories(com.refractor.core.services.log PUBLIC include)
target_include_directories(com.refractor.core.services.log PRIVATE src)

file(GLOB_RECURSE log_headers "include/*.h")
file(GLOB_RECURSE log_sources "src/*.h" "src/*.c")

target_sources(com.refractor.core.services.log PUBLIC ${log_headers})
target_sources(com.refractor.core.services.log PRIVATE ${log_sources})

target_link_libraries(com.refractor.core.services.log PUBLIC com.refractor.core.foundation)
