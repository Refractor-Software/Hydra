# Auto-discover all C++ runtime modules in this directory.
# A valid module directory must contain a Module.cmake marker file.
# To add a new module, create its directory with Module.cmake — no edits here needed.

hydra_discover_modules("${CMAKE_CURRENT_SOURCE_DIR}")
