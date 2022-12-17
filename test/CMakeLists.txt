###########################
# Common test configuration
###########################

# Libraries all tests depend on
find_package(CMOCKA REQUIRED)
find_package(FUSE3 REQUIRED)

# Include header files from the following paths
include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src
)

# Define a snapshot version to generate the version file
# if it doesn't exist
set(SNAPSHOT_VERSION test)
configure_file(usbi3c_version_i_test.h.in "${CMAKE_CURRENT_BINARY_DIR}/usbi3c_version_i.h" @ONLY)

# Add directories with tests
add_subdirectory(unit)
add_subdirectory(system)

# Custom target "check_style" for static analysis
add_custom_target(check_style
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/code_analysis/check_style.sh
  COMMENT "Checking code style...\n\n"
)
