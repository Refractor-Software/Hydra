add_executable(hydra_win64 WIN32)

target_include_directories(hydra_win64 PRIVATE src)

file(GLOB_RECURSE hydra_win64_sources "src/*.h" "src/*.c")

target_sources(hydra_win64 PRIVATE ${hydra_win64_sources})

target_link_libraries(hydra_win64 PRIVATE foundation application input log user32 gdi32 xinput d3d12 dxgi dxguid dbghelp winmm ole32 shell32)
