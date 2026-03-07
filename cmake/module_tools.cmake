function(hydra_add_module _module_name)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    set(_current_include_path "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(_current_src_path "${CMAKE_CURRENT_SOURCE_DIR}/src")

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
        # NOTE(will) Not strictly necessary but this enables headers to exist in CLion clangd/clang-tidy by default.
        file(GLOB_RECURSE MODULE_INCLUDE
            "${_current_include_path}/*.hpp"
            "${_current_include_path}/*.h"
            "${_current_include_path}/*.inl"
        )
        target_sources(${_module_name} PUBLIC ${MODULE_INCLUDE})
        target_include_directories(${_module_name} PUBLIC ${_current_include_path})
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

        # Static CRT — debug uses /MTd, all others use /MT
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MTd>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Debug>>>:/MT>

        # Whole program optimization for Shipping only
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/GL>
    )

    # Linker flag required to pair with /GL
    target_link_options(${_module_name} PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
    )

    # --- Preprocessor definitions ---
    target_compile_definitions(${_module_name} PRIVATE
        # Unicode character set
        UNICODE
        _UNICODE

        # Suppress min/max macros from windows.h
        NOMINMAX

        # Disable exceptions in Abseil
        ABSL_NO_EXCEPTIONS

        # Disable exceptions in STL (pairs with /EHs-c-)
        $<$<CXX_COMPILER_ID:MSVC>:_HAS_EXCEPTIONS=0>
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
    set_target_properties(${_module_name} PROPERTIES FOLDER "modules")

endfunction()