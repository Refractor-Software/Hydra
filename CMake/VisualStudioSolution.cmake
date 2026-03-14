# vs_solution.cmake
# Visual Studio solution-level settings. Only meaningful for the VS generator.
if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    return()
endif()

# Suppress the default ALL_BUILD and ZERO_CHECK noise projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_cmake")

# --- C# projects ---
include("${CMAKE_CURRENT_LIST_DIR}/HydraStudio.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/HydraTools.cmake")