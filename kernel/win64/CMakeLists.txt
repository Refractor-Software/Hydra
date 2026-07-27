add_executable(com.refractor.kernel.win64 WIN32)

#[[
    The target is named for where it lives in the build tree; the binary is named for the product
    that ships.
#]]
set_target_properties(com.refractor.kernel.win64 PROPERTIES OUTPUT_NAME Hydra)

target_include_directories(com.refractor.kernel.win64 PRIVATE src)

file(GLOB_RECURSE win64_sources CONFIGURE_DEPENDS "src/*.h" "src/*.c")

target_sources(com.refractor.kernel.win64 PRIVATE ${win64_sources})

target_link_libraries(com.refractor.kernel.win64 PRIVATE
    com.refractor.core.foundation
    com.refractor.core.application
    com.refractor.core.services.input
    com.refractor.core.services.log
    user32
    gdi32
    xinput
    d3d12
    dxgi
    dxguid
    dbghelp
    winmm
    ole32
    shell32)
