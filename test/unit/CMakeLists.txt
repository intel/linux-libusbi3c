#########################
# Unit test configuration
#########################

# Include header files from the following paths
include_directories(
  mocks
)

# wrap functions listed in wrap.txt so they can be mocked
file(STRINGS wrap.txt wrap_functions)
foreach(func ${wrap_functions})
  add_link_options(-Wl,--wrap=${func})
  message(STATUS "${func} is wrapped in tests")
endforeach()

# include all unit test mocks and helper files along with the
# project source files as source files for tests
file(GLOB test_mocks mocks/*.c)
set(testsource ${test_helpers} ${test_mocks} ${c_sources})

set(test_files
  test_basic.c
  test_bulk_transfer_cancel_request_async.c
  test_bulk_transfer_get_response.c
  test_bulk_transfer_get_response_multiple_commands.c
  test_bulk_transfer_get_response_vendor_specific.c
  test_bulk_transfer_resume_request_async.c
  test_bulk_transfer_search_response.c
  test_bulk_transfer_send_commands.c
  test_device_send_request_to_i3c_controller.c
  test_ibi_notification.c
  test_ibi_response_queue.c
  test_list_concat.c
  test_list_free.c
  test_list_len.c
  test_list_free_matching_nodes.c
  test_list_free_node.c
  test_list_search.c
  test_notification_address_change.c
  test_notification_stall_on_nack.c
  test_table_identify_devices.c
  test_table_fill_from_device_table_buffer.c
  test_table_get_devices.c
  test_table_update_target_device_info.c
  test_target_device.c
  test_target_device_table.c
  test_target_reset.c
  test_usb_context_init_deinit.c
  test_usb_context_find_devices.c
  test_usb_device_bulk_transfer.c
  test_usb_device_bulk_transfer_polling.c
  test_usb_device_control_transfer.c
  test_usb_device_control_transfer_async.c
  test_usb_device_get_max_bulk_response_buffer.c
  test_usb_device_init_deinit.c
  test_usb_device_interrupt_transfer.c
  test_usbi3c_add_device_to_table.c
  test_usbi3c_change_i3c_device_address.c
  test_usbi3c_device_is_active_controller.c
  test_usbi3c_disable_feature.c
  test_usbi3c_enable_feature.c
  test_usbi3c_enqueue_command.c
  test_usbi3c_get_address_list.c
  test_usbi3c_get_device_address.c
  test_usbi3c_get_devices.c
  test_usbi3c_get_device_role.c
  test_usbi3c_get_i3c_mode.c
  test_usbi3c_get_request_reattempt_max.c
  test_usbi3c_get_target_device_config.c
  test_usbi3c_get_target_device_max_ibi_payload.c
  test_usbi3c_get_target_device_table.c
  test_usbi3c_get_target_info.c
  test_usbi3c_get_timeout.c
  test_usbi3c_init.c
  test_usbi3c_initialize_controller.c
  test_usbi3c_initialize_secondary_controller.c
  test_usbi3c_initialize_target_device.c
  test_usbi3c_notifications.c
  test_usbi3c_on_controller_event.c
  test_usbi3c_on_vendor_specific_response.c
  test_usbi3c_request_i3c_controller_role.c
  test_usbi3c_send_commands.c
  test_usbi3c_set_target_device_config.c
  test_usbi3c_set_target_device_max_ibi_payload.c
  test_usbi3c_submit_commands.c
  test_usbi3c_submit_vendor_specific_request.c
)

add_definitions(-DUNIT_TESTING_ASSERTION=ON)

# add the "unit_test" target
add_custom_target(
  unit_test
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMENT "Running unit tests...\n\n"
)

# build the test files
foreach(test_file ${test_files})
  get_filename_component(filename ${test_file} NAME_WE)
  add_executable(${filename} ${test_file} ${testsource})
  target_link_libraries(${filename} ${USB1_LIBRARIES} ${CMOCKA_LIBRARIES} Threads::Threads)
  add_test(${filename} ${CMAKE_CURRENT_BINARY_DIR}/${filename})
  add_dependencies(unit_test ${filename})
endforeach()
