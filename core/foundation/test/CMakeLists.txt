add_executable(com.refractor.core.foundation.test)

target_include_directories(com.refractor.core.foundation.test PRIVATE .)

file(GLOB_RECURSE foundation_test_sources CONFIGURE_DEPENDS "*.h" "*.c")

target_sources(com.refractor.core.foundation.test PRIVATE ${foundation_test_sources})

#[[
    Foundation is platform-independent by design: it *declares* RE_VirtualMemory_* and RE_Thread_*
    and the platform kernel defines them. A test binary therefore has to supply a platform side or
    it will not link.

    It compiles the real win64 implementations rather than a stub, because those implementations
    are a large part of what is being tested. If the kernel is ever split into a library and a thin
    executable, this becomes a plain target_link_libraries and the include path goes away.
#]]
target_include_directories(com.refractor.core.foundation.test PRIVATE ${CMAKE_SOURCE_DIR}/kernel/win64/src)

target_sources(com.refractor.core.foundation.test PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel/win64/src/RE/Win64/Win64VirtualMemory.c
    ${CMAKE_SOURCE_DIR}/kernel/win64/src/RE/Win64/Win64Thread.c)

target_link_libraries(com.refractor.core.foundation.test PRIVATE com.refractor.core.foundation)

add_test(NAME com.refractor.core.foundation.test COMMAND com.refractor.core.foundation.test)
