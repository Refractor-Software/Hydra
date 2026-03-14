# vs_solution.cmake
# Visual Studio solution-level settings. Only meaningful for the VS generator.
if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    return()
endif()

# Suppress the default ALL_BUILD and ZERO_CHECK noise projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_cmake")

# Solution-level startup project
#set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Hydra)