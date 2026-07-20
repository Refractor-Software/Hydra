add_executable(hydra-win64 WIN32)

target_include_directories(hydra-win64 PRIVATE ../)
target_sources(hydra-win64 PRIVATE
    win64_audio.c
    win64_input.c
    win64_main.c
    win64_plugin.c
    win64_render.c
    win64_storage.c
    win64_studiolink.c
    win64_thread.c
)
