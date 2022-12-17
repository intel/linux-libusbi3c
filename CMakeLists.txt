# -------------------------------------
# Project
# -------------------------------------
project(libusbi3c C)
set(MAJOR_VERSION 0)
set(MINOR_VERSION 1)
set(VERSION_STRING ${MAJOR_VERSION}.${MINOR_VERSION})
set(VERSION ${VERSION_STRING})
set(PACKAGE libusbi3c)
set(OFFICIAL_PROJECT_NAME "USB I3C* Library for Linux* OS")
set(CMAKE_C_COMPILER gcc)

# -------------------------------------
# CMake
# -------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_COLOR_MAKEFILE ON)
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)


# -------------------------------------
# Build Types
# -------------------------------------
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel tsan asan lsan msan ubsan"
    FORCE)

# Default build type
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Debug
set(CMAKE_C_FLAGS_DEBUG
    "-DDEBUG -g -O0"
    CACHE STRING "Flags used by the C compiler during Debug builds."
    FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O0"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
    FORCE)

# ThreadSanitizer
set(CMAKE_C_FLAGS_TSAN
    "-fsanitize=thread -g -O0"
    CACHE STRING "Flags used by the C compiler during ThreadSanitizer builds."
    FORCE)

# LeakSanitizer
set(CMAKE_C_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O0"
    CACHE STRING "Flags used by the C compiler during LeakSanitizer builds."
    FORCE)

# UndefinedBehaviour
set(CMAKE_C_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C compiler during UndefinedBehaviourSanitizer builds."
    FORCE)


# -------------------------------------
# Common configuration
# -------------------------------------
# Define GNU standard installation directories
include(GNUInstallDirs)

# Include dependent libraries:
# libusb
find_package(USB1 REQUIRED)
include_directories(${USB1_INCLUDE_DIR})
# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add a "make dist" target
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${VERSION_STRING})
add_custom_target(
  dist
  COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Add an "uninstall" target
if(NOT TARGET uninstall)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uninstall.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/Uninstall.cmake @ONLY)
  add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/Uninstall.cmake)
endif()

# Add a "compliant" target
add_custom_target(
	compliant
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/code_analysis/check_style.sh
	COMMENT "Running code style check...\n\n"
)

# build
add_definitions(-Wall -Werror)
add_subdirectory(src)
add_subdirectory(packages)


# -------------------------------------
# Optional configuration
# -------------------------------------
include(CMakeOptions.txt)

# Enable documentation if option selected
if(DOCUMENTATION)
  find_package(Doxygen REQUIRED)

  # Copy doxy.config.in
  set(top_srcdir ${CMAKE_SOURCE_DIR})
  configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)

  # Run doxygen
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/html/index.html
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
    DEPENDS ${c_headers};${c_sources};${cpp_sources};${cpp_headers}
  )

  add_custom_target(docs ALL DEPENDS ${CMAKE_BINARY_DIR}/doc/html/index.html)
endif()

# Enable testing if option selected
if(BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(test)
endif()

# Enable test coverage if option selected
if(COVERAGE AND BUILD_TESTS)
  set(CMAKE_C_FLAGS
    "--coverage"
    CACHE STRING "Adds the test coverage flag to the compiler options"
    FORCE)

  add_custom_target(coverage
    # remove the tracefiles from previous runs and reset the counters
    COMMAND rm -rf coverage
    COMMAND mkdir -p coverage
    COMMAND lcov --zerocounters --directory .
    #  create an lcov "baseline" before running any tests, this will allow us
    #  to include source files that were not even loaded during the tests
    COMMAND lcov --no-external --capture --initial --directory ${CMAKE_SOURCE_DIR} --output-file coverage/coverage_base.info
    # run the tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir test/unit
    # coverage after tests
    COMMAND lcov --no-external --capture --directory ${CMAKE_SOURCE_DIR} --output-file coverage/coverage_after_test.info
    #  combine the 'before tests' and 'after tests' snapshots
    COMMAND lcov --add-tracefile coverage/coverage_base.info --add-tracefile coverage/coverage_after_test.info --output-file coverage/coverage.info
    # remove the stuff that we aren't interested in
    COMMAND lcov --remove coverage/coverage.info '*/test/*' '*/examples/*' --output-file coverage/coverage.info
    COMMAND genhtml coverage/coverage.info --output-directory coverage
    # these dependencies ensure that the code is built before running this target
    DEPENDS unit_test
    COMMENT "Analyzing coverage...\n\n"
  )
endif()

# Enable the code samples if option selected
if(BUILD_SAMPLES)
  add_subdirectory(examples)
endif()

# -------------------------------------
# Configure generators for binary
# installers and source packages
# -------------------------------------
include(CPack)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${VERSION_STRING})
set(CPACK_PACKAGE_DESCRIPTION "libusbi3c library")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CPACK_PACKAGE_DESCRIPTION})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_IGNORE_FILES "\\\\.git;~$;build/")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

if(UNIX)
  set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PACK_ARCH})
  set(CPACK_GENERATOR "DEB;RPM")
  set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
  set(CPACK_SET_DESTDIR ON)
endif()

if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win32")
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_CMAKE_GENERATOR "MinGW Makefiles")
  set(CPACK_PACKAGE_VENDOR "")
  set(CPACK_NSIS_DISPLAY_NAME "libusbi3c")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "libusbi3c")
  set(CPACK_NSIS_MODIFY_PATH ON)
endif()

if(NOT APPLE)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PACK_ARCH "")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PACK_ARCH .x86_64)
  endif()
else()
  set(PACK_ARCH "")
endif()

# Set components
set(CPACK_COMPONENTS_ALL sharedlibs staticlibs headers)
set(CPACK_COMPONENT_SHAREDLIBS_DISPLAY_NAME "Shared libraries")
set(CPACK_COMPONENT_SHAREDLIBS_DESCRIPTION "Shared library for general use")
set(CPACK_COMPONENT_SHAREDLIBS_GROUP "Development")
set(CPACK_COMPONENT_STATICLIBS_DISPLAY_NAME "Static libraries")
set(CPACK_COMPONENT_STATICLIBS_DESCRIPTION "Static library, good if you want to embed libusbi3c in your application")
set(CPACK_COMPONENT_STATICLIBS_GROUP "Development")
set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "C headers")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "C header files")
set(CPACK_COMPONENT_HEADERS_GROUP "Development")

# PkgConfig
if(NOT DEFINED LIBUSBI3C_CMAKE_CONFIG_DIR)
  set(LIBUSBI3C_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/libusbi3c)
endif()

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/LibUSBI3CConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/LibUSBI3CConfigVersion.cmake
  cmake/UseLibUSBI3C.cmake
  DESTINATION ${LIBUSBI3C_CMAKE_CONFIG_DIR}
)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_FULL_BINDIR})
set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${PROJECT_NAME})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
configure_file(${CMAKE_SOURCE_DIR}/libusbi3c.pc.in ${CMAKE_BINARY_DIR}/libusbi3c.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libusbi3c.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(UNIX OR MINGW)
  configure_file(libusbi3c-config.in ${CMAKE_CURRENT_BINARY_DIR}/libusbi3c-config @ONLY)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/libusbi3c-config
          DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(LIBUSBI3C_INCLUDE_DIR ${includedir})
set(LIBUSBI3C_INCLUDE_DIRS ${LIBUSBI3C_INCLUDE_DIR})
set(LIBUSBI3C_LIBRARY usbi3c)
set(LIBUSBI3C_LIBRARIES ${LIBUSBI3C_LIBRARY})
list(APPEND LIBUSBI3C_LIBRARIES ${USB1_LIBRARIES})
set(LIBUSBI3C_STATIC_LIBRARY usbi3c.a)
set(LIBUSBI3C_STATIC_LIBRARIES ${LIBUSBI3C_STATIC_LIBRARY})
list(APPEND LIBUSBI3C_STATIC_LIBRARIES ${USB1_LIBRARIES})
set(LIBUSBI3C_LIBRARY_DIRS ${libdir})
set(LIBUSBI3C_ROOT_DIR ${prefix})
set(LIBUSBI3C_VERSION_STRING ${VERSION_STRING})
set(LIBUSBI3C_VERSION_MAJOR ${MAJOR_VERSION})
set(LIBUSBI3C_VERSION_MINOR ${MINOR_VERSION})
set(LIBUSBI3C_USE_FILE ${CMAKE_INSTALL_PREFIX}/${LIBUSBI3C_CMAKE_CONFIG_DIR}/UseLibUSBI3C.cmake)

if(CMAKE_VERSION VERSION_LESS 2.8.8)
  configure_file(cmake/LibUSBI3CConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/LibUSBI3CConfig.cmake @ONLY)
  configure_file(cmake/LibUSBI3CConfigVersion.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/LibUSBI3CConfigVersion.cmake @ONLY)
else()
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    cmake/LibUSBI3CConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LibUSBI3CConfig.cmake
    INSTALL_DESTINATION ${LIBUSBI3C_CMAKE_CONFIG_DIR}
    PATH_VARS
      LIBUSBI3C_USE_FILE
      LIBUSBI3C_ROOT_DIR
      LIBUSBI3C_INCLUDE_DIR
      LIBUSBI3C_INCLUDE_DIRS
      LIBUSBI3C_LIBRARY_DIRS
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
  )
  write_basic_package_version_file(
    LibUSBI3CConfigVersion.cmake
    VERSION ${LIBUSBI3C_VERSION_STRING}
    COMPATIBILITY AnyNewerVersion
  )
endif()


# -------------------------------------
# Summary
# -------------------------------------
message (STATUS
  "
  ------------------------------------------
  Summary of build options:
  ------------------------------------------
  Build static libs: ${STATICLIBS}
  Build tests: ${BUILD_TESTS}
  Build test coverage: ${COVERAGE}
  Build samples: ${BUILD_SAMPLES}
  Build API documentation: ${DOCUMENTATION}
  Build type: ${CMAKE_BUILD_TYPE}
  ------------------------------------------
"
)
