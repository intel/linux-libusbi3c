###########################
# System test configuration
###########################

# include all system test helper files along with the
# project source files as source files for tests
file(GLOB test_helpers helpers/*.c)
set(testsource ${test_helpers})

set(test_files
  test_add_i3c_device_to_table.c
  test_change_i3c_device_address.c
  test_device_is_active_controller.c
  test_disable_features.c
  test_enable_features.c
  test_get_address_list.c
  test_get_device_role.c
  test_get_target_bcr.c
  test_get_target_dcr.c
  test_get_target_type.c
  test_initialize_device.c
  test_on_bus_error.c
  test_on_controller_event.c
  test_on_hotjoin.c
  test_on_ibi.c
  test_request_i3c_controller_role.c
  test_send_commands.c
  test_set_i3c_mode.c
  test_set_request_reattempt_max.c
  test_set_target_device_config.c
  test_set_target_device_max_ibi_payload.c
  test_set_timeout.c
  test_submit_commands.c
  test_submit_vendor_specific_request.c
)

# add the "system_test" target
add_custom_target(
  system_test
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMENT "Running system tests...\n\n"
)

# build the test files
foreach(test_file ${test_files})
  get_filename_component(filename ${test_file} NAME_WE)
  add_executable(${filename} ${test_file} ${testsource})
  target_link_libraries(${filename} ${CMOCKA_LIBRARIES} usbi3c Threads::Threads)
  add_test(${filename} ${CMAKE_CURRENT_BINARY_DIR}/${filename})
  add_dependencies(system_test ${filename})
endforeach()
