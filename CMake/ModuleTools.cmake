function(hydra_add_module _module_name)
    cmake_parse_arguments(ARG "" "EXPORT" "DEPENDS" ${ARGN})

    if (DEFINED HYDRA_CURRENT_PACKAGE_EXPORT)
        set(_export_set "${HYDRA_CURRENT_PACKAGE_EXPORT}")
    else()
        set(_export_set "HydraTargets")
    endif()

    set(_current_include_path "${CMAKE_CURRENT_SOURCE_DIR}/Include")
    set(_current_src_path "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    add_library(${_module_name} STATIC)

    # --- Sources ---
    if (EXISTS ${_current_src_path})
        file(GLOB_RECURSE MODULE_SRC
            "${_current_src_path}/*.cpp"
            "${_current_src_path}/*.c"
            "${_current_src_path}/*.hpp"
            "${_current_src_path}/*.h"
            "${_current_src_path}/*.inl"
        )
        target_sources(${_module_name} PRIVATE ${MODULE_SRC})
    endif ()

    if (EXISTS ${_current_include_path})
        file(GLOB_RECURSE MODULE_INCLUDE
            "${_current_include_path}/*.hpp"
            "${_current_include_path}/*.h"
            "${_current_include_path}/*.inl"
        )
        target_sources(${_module_name} PRIVATE
            $<BUILD_INTERFACE:${MODULE_INCLUDE}>
        )
        target_include_directories(${_module_name} PUBLIC
            $<BUILD_INTERFACE:${_current_include_path}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
    endif ()
    target_include_directories(${_module_name} PRIVATE ${_current_src_path})

    # --- C++ standard ---
    target_compile_features(${_module_name} PUBLIC cxx_std_20)
    set_target_properties(${_module_name} PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # --- Compiler settings (MSVC) ---
    target_compile_options(${_module_name} PRIVATE
        # Warning level 4, warnings as errors
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>

        # Disable RTTI
        $<$<CXX_COMPILER_ID:MSVC>:/GR->

        # Disable exceptions
        $<$<CXX_COMPILER_ID:MSVC>:/EHs-c->

        # Correct __cplusplus results
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>

        # Static CRT
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MTd>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Debug>>>:/MT>

        # Whole program optimization for Release (Shipping) only
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/GL>
    )

    # Linker flag required to pair with /GL
    target_link_options(${_module_name} PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
    )

    # --- Preprocessor definitions ---
    target_compile_definitions(${_module_name} PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        ABSL_NO_EXCEPTIONS
        $<$<CXX_COMPILER_ID:MSVC>:_HAS_EXCEPTIONS=0>

        # Per-config defines
        $<$<CONFIG:Debug>:HYDRA_DEBUG>
        $<$<CONFIG:RelWithDebInfo>:HYDRA_DEVELOPMENT>
        $<$<CONFIG:Release>:HYDRA_SHIPPING>
    )

    # --- Architecture / SIMD flags ---
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(${_module_name} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2 -mfma -msse4.2>
        )
        target_compile_definitions(${_module_name} PRIVATE
            HYDRA_SIMD_AVX2
            HYDRA_SIMD_FMA
            HYDRA_SIMD_SSE42
        )
    endif ()

    # --- Dependencies ---
    if (ARG_DEPENDS)
        target_link_libraries(${_module_name} PRIVATE ${ARG_DEPENDS})
    endif ()

    # --- IDE ---
    set_target_properties(${_module_name} PROPERTIES
        FOLDER "Runtime"
        VS_GLOBAL_UseDebugLibraries "$<CONFIG:Debug>"
    )

    if (MODULE_SRC)
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${MODULE_SRC})
    endif()
    if (MODULE_INCLUDE)
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${MODULE_INCLUDE})
    endif()

    # --- Install ---
    install(TARGETS ${_module_name}
        EXPORT ${_export_set}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${_current_include_path}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endfunction()

# Adds a single C# project to the Visual Studio solution.
#
#   hydra_add_csproj(<path>
#       [FOLDER <folder>]    # VS solution folder, e.g. "Studio" or "Packages/Foo"
#       [PLATFORM <guid>]    # override VS project type GUID (default: C# GUID)
#   )
#
# <path> may be:
#   - a .csproj file (absolute or relative to CMAKE_CURRENT_SOURCE_DIR)
#   - a directory containing exactly one .csproj file
#
# No-op when not using a Visual Studio generator.
function(hydra_add_csproj _path)
    if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        return()
    endif()

    cmake_parse_arguments(ARG "" "FOLDER;PLATFORM" "" ${ARGN})

    # Resolve to an absolute path
    if (NOT IS_ABSOLUTE "${_path}")
        set(_path "${CMAKE_CURRENT_SOURCE_DIR}/${_path}")
    endif()

    # Accept either a directory or a .csproj file directly
    if (IS_DIRECTORY "${_path}")
        file(GLOB _csproj "${_path}/*.csproj")
        list(LENGTH _csproj _count)
        if (_count EQUAL 0)
            message(WARNING "hydra_add_csproj: No .csproj found in '${_path}', skipping.")
            return()
        elseif (_count GREATER 1)
            message(WARNING "hydra_add_csproj: Multiple .csproj files found in '${_path}', skipping.")
            return()
        endif()
    else()
        set(_csproj "${_path}")
        if (NOT EXISTS "${_csproj}")
            message(WARNING "hydra_add_csproj: '${_csproj}' does not exist, skipping.")
            return()
        endif()
    endif()

    get_filename_component(_name "${_csproj}" NAME_WE)

    if (ARG_PLATFORM)
        set(_type_guid "${ARG_PLATFORM}")
    else()
        set(_type_guid "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}")
    endif()

    include_external_msproject(${_name} "${_csproj}" TYPE "${_type_guid}")

    if (ARG_FOLDER)
        set_target_properties(${_name} PROPERTIES FOLDER "${ARG_FOLDER}")
    endif()
endfunction()

# Discovers all C# projects under a directory and adds them to the VS solution.
#
#   hydra_discover_csprojs(<directory>
#       [FOLDER <folder>]   # VS solution folder applied to every discovered project
#       [RECURSE]           # search subdirectories (default: top-level subdirs only)
#   )
#
# Each immediate subdirectory of <directory> that contains exactly one .csproj
# file is registered via hydra_add_csproj(). With RECURSE, all .csproj files
# anywhere under <directory> are discovered instead.
#
# No-op when not using a Visual Studio generator.
function(hydra_discover_csprojs _dir)
    if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        return()
    endif()

    cmake_parse_arguments(ARG "RECURSE" "FOLDER" "" ${ARGN})

    if (NOT IS_ABSOLUTE "${_dir}")
        set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
    endif()

    if (NOT EXISTS "${_dir}")
        message(WARNING "hydra_discover_csprojs: '${_dir}' does not exist, skipping.")
        return()
    endif()

    if (ARG_RECURSE)
        file(GLOB_RECURSE _csprojs "${_dir}/*.csproj")
    else()
        # One .csproj per immediate subdirectory
        file(GLOB _subdirs LIST_DIRECTORIES true "${_dir}/*")
        set(_csprojs "")
        foreach(_sub IN LISTS _subdirs)
            if (IS_DIRECTORY "${_sub}")
                file(GLOB _found "${_sub}/*.csproj")
                list(APPEND _csprojs ${_found})
            endif()
        endforeach()
    endif()

    foreach(_csproj IN LISTS _csprojs)
        if (ARG_FOLDER)
            hydra_add_csproj("${_csproj}" FOLDER "${ARG_FOLDER}")
        else()
            hydra_add_csproj("${_csproj}")
        endif()
    endforeach()
endfunction()

function(hydra_declare_package)
    cmake_parse_arguments(ARG "" "NAME" "RUNTIME;STUDIO;TOOLS" ${ARGN})

    if (NOT ARG_NAME)
        message(FATAL_ERROR "hydra_declare_package: NAME is required.")
    endif()

    message(STATUS "Hydra: registering package '${ARG_NAME}'")

    # --- Runtime modules ---
    set(HYDRA_CURRENT_PACKAGE_EXPORT "Hydra${ARG_NAME}Targets")
    foreach(_runtime_module IN LISTS ARG_RUNTIME)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_runtime_module}")
            add_subdirectory(${_runtime_module})
        else()
            message(WARNING "hydra_declare_package: Runtime module '${_runtime_module}' not found, skipping.")
        endif()
    endforeach()
    unset(HYDRA_CURRENT_PACKAGE_EXPORT)

    if (ARG_RUNTIME)
        install(EXPORT "Hydra${ARG_NAME}Targets"
            FILE "${ARG_NAME}Targets.cmake"
            NAMESPACE Hydra::
            DESTINATION "Packages/${ARG_NAME}/Runtime"
        )
    endif()

    # --- Tool modules (C++ or C# CLI tools) ---
    foreach(_tool IN LISTS ARG_TOOLS)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_tool}")
            add_subdirectory(${_tool})
        else()
            message(WARNING "hydra_declare_package: Tool '${_tool}' not found, skipping.")
        endif()
    endforeach()

    # --- Studio modules (C# — VS solution inclusion only) ---
    foreach(_studio_module IN LISTS ARG_STUDIO)
        set(_studio_path "${CMAKE_CURRENT_SOURCE_DIR}/${_studio_module}")
        if (NOT EXISTS "${_studio_path}")
            message(WARNING "hydra_declare_package: Studio module '${_studio_module}' not found, skipping.")
            continue()
        endif()
        hydra_add_csproj("${_studio_path}" FOLDER "Packages/${ARG_NAME}")
    endforeach()

    # --- Install: Studio module DLLs ---
    foreach(_studio_module IN LISTS ARG_STUDIO)
        set(_studio_path "${CMAKE_CURRENT_SOURCE_DIR}/${_studio_module}")
        if (EXISTS "${_studio_path}")
            install(DIRECTORY "${_studio_path}/"
                DESTINATION "Packages/${ARG_NAME}/Studio"
                FILES_MATCHING PATTERN "*.dll"
            )
        endif()
    endforeach()

    # --- Track globally ---
    set_property(GLOBAL APPEND PROPERTY HYDRA_REGISTERED_PACKAGES ${ARG_NAME})
endfunction()