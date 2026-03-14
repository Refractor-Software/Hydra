include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Generate and install the targets file
install(EXPORT HydraTargets
    FILE HydraTargets.cmake
    NAMESPACE Hydra::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hydra
)

# Generate the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/HydraConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HydraConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hydra
)

# Generate the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HydraConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HydraConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HydraConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hydra
)