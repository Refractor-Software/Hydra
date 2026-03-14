# Auto-discover and include all packages in this directory.
# A valid package directory must be prefixed with "Package." and contain a Package.cmake file.

file(GLOB _package_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/Package.*")

foreach(_pkg_dir IN LISTS _package_dirs)
    if (NOT IS_DIRECTORY "${_pkg_dir}")
        continue()
    endif()

    set(_pkg_cmake "${_pkg_dir}/Package.cmake")
    if (NOT EXISTS "${_pkg_cmake}")
        message(WARNING "Hydra: directory '${_pkg_dir}' looks like a package but has no Package.cmake, skipping.")
        continue()
    endif()

    include("${_pkg_cmake}")
endforeach()