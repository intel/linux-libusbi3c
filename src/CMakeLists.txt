# Includes
include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Version information
set(SNAPSHOT_VERSION "unknown")
execute_process(
  COMMAND git describe --always
  OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
  RESULT_VARIABLE GIT_DESCRIBE_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(${GIT_DESCRIBE_RESULT} STREQUAL 0)
  set(SNAPSHOT_VERSION ${GIT_DESCRIBE_OUTPUT})
endif()
message(STATUS "Detected git snapshot version: ${SNAPSHOT_VERSION}")

configure_file(usbi3c_version_i.h.in "${CMAKE_CURRENT_BINARY_DIR}/usbi3c_version_i.h" @ONLY)

# Targets
set(c_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/bulk_transfer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ibi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ibi_response.c
  ${CMAKE_CURRENT_SOURCE_DIR}/list.c
  ${CMAKE_CURRENT_SOURCE_DIR}/target_device.c
  ${CMAKE_CURRENT_SOURCE_DIR}/target_device_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usbi3c.c
  CACHE INTERNAL "List of c sources"
)

set(c_headers
  ${CMAKE_CURRENT_SOURCE_DIR}/usbi3c.h
  ${CMAKE_CURRENT_SOURCE_DIR}/usbi3c_commands.h
  CACHE INTERNAL "List of c headers"
)

add_library(usbi3c SHARED ${c_sources})

math(EXPR VERSION_FIXUP "${MAJOR_VERSION} + 1")    # Compatibility with previous releases
set_target_properties(usbi3c PROPERTIES VERSION ${VERSION_FIXUP}.${MINOR_VERSION}.0 SOVERSION 2)
# Prevent clobbering each other during the build
set_target_properties (usbi3c PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# Dependencies
target_link_libraries(usbi3c ${USB1_LIBRARIES} Threads::Threads)

install(TARGETS usbi3c
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib${LIB_SUFFIX}
  ARCHIVE DESTINATION lib${LIB_SUFFIX}
)

if(STATICLIBS)
  add_library(usbi3c-static STATIC ${c_sources})
  target_link_libraries(usbi3c-static ${USB1_LIBRARIES})
  set_target_properties(usbi3c-static PROPERTIES OUTPUT_NAME "usbi3c")
  set_target_properties(usbi3c-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  install(TARGETS usbi3c-static
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    COMPONENT staticlibs
  )
endif()

install(FILES ${c_headers}
  DESTINATION include/${PROJECT_NAME}
  COMPONENT headers
)
