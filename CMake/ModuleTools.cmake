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
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        foreach(_studio_module IN LISTS ARG_STUDIO)
            set(_studio_path "${CMAKE_CURRENT_SOURCE_DIR}/${_studio_module}")
            if (NOT EXISTS "${_studio_path}")
                message(WARNING "hydra_declare_package: Studio module '${_studio_module}' not found, skipping.")
                continue()
            endif()

            file(GLOB _csproj "${_studio_path}/*.csproj")
            if (NOT _csproj)
                message(WARNING "hydra_declare_package: No .csproj found in '${_studio_path}', skipping.")
                continue()
            endif()

            get_filename_component(_proj_name "${_csproj}" NAME_WE)
            include_external_msproject(
                ${_proj_name}
                "${_csproj}"
                TYPE "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"
            )
            set_target_properties(${_proj_name} PROPERTIES
                FOLDER "Packages/${ARG_NAME}"
            )
        endforeach()
    endif()

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