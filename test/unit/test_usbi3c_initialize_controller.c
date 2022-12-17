/***************************************************************************
  USBI3C  -  Library to talk to I3C devices via USB.
  -------------------
  copyright            : (C) 2021 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation.             *
 *                                                                         *
 ***************************************************************************/

#include "helpers.h"
#include "mocks.h"

int fake_handle = 1;
const int DEVICE_CAPABILITY = 0b000000000111;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, &fake_handle);
	free(deps);

	return 0;
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_initialize_device(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles a failure getting the bus capabilities gracefully */
static void test_negative_get_capability_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int NUMBER_OF_DEVICES = 0;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY
	 * to fake an error */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_FAILURE);
	assert_non_null(buffer);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Negative test to validate the function handles an unknown error getting the capability data gracefully */
static void test_negative_unknown_error_getting_capability_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int NUMBER_OF_DEVICES = 0;
	const int BAD_CAPABILITY_STATE = 0x01;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY
	 * to fake an error */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, BAD_CAPABILITY_STATE, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Negative test to validate the function handles a failure initializing the response buffer gracefully */
static void test_negative_response_buffer_init_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int NO_ENTRIES = 0;
	const int DEVICE_CAPABILITY = 0b000000000111;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NO_ENTRIES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a failure initializing the bulk response buffer */
	mock_libusb_get_max_packet_size(RETURN_FAILURE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);

	free(buffer);
}

/* Negative test to validate the function handles a failure starting the response polling gracefully */
static void test_negative_bulk_transfer_polling_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int NO_ENTRIES = 0;
	const int DEVICE_CAPABILITY = 0b000000000111;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NO_ENTRIES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* to mock a failure initializing the async bulk transfer polling we
	 * don't need to do anything else since we already defined that
	 * libusb_submit_transfer is going to succeed once and then it is going
	 * to fail the next time, which will cause the bulk transfer polling
	 * init to fail */
	/* mock a failure submitting initializing the async bulk transfer required
	 * for the polling */
	mock_libusb_submit_transfer(LIBUSB_ERROR_ACCESS);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	free(buffer);
}

/*******************************************************************************
 * From here on all tests only apply to the I3C device in the controller role  *
 *******************************************************************************/

/* Negative test to validate the function handles a failure initializing interrupts gracefully */
static void test_negative_interrupt_init_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int SUCCEED_ONCE = 1;
	const int NUMBER_OF_DEVICES = 0;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* fake a failure initializing the interrupt handler */
	mock_libusb_submit_transfer(SUCCEED_ONCE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Negative test to validate the function handles an error initializing the I3C bus gracefully */
static void test_negative_i3c_bus_init_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int NO_ENTRIES = 0;
	const int DEVICE_CAPABILITY = 0b000000000111;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NO_ENTRIES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* to mock a failure initializing the i3c bus we need to mock libusb_control_transfer
	 * to fake an error */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_FAILURE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.active_i3c_controller, FALSE);

	free(buffer);
}

/* Negative test to validate the function handles a failure initializing the I3C bus gracefully */
static void test_negative_i3c_bus_init_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int INTERRUPT_ENDPOINT = 3;
	const int NO_ENTRIES = 0;
	const int DEVICE_CAPABILITY = 0b000000000111;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NO_ENTRIES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a failure in bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = FAILURE_DEVICE_DISCOVERY_N_DYNAMIC_ADDRESS_ASSIGNMENT;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.active_i3c_controller, FALSE);

	free(buffer);
	free(notification);
}

/* Negative test to validate the function handles a failure getting the device table gracefully */
static void test_negative_target_device_table_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *target_device_table_buffer = NULL;
	int ret = 0;
	const int INTERRUPT_ENDPOINT = 3;
	const int NUMBER_OF_DEVICES = 3;
	const int DEVICE_CAPABILITY = 0b000000000111;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a successful bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	/* fake an error getting the target device table */
	target_device_table_buffer = mock_get_target_device_table(&fake_handle, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_FAILURE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	free(buffer);
	free(notification);
	free(target_device_table_buffer);
}

/* Negative test to validate the I3C controller handle the config request failure gracefully */
static void test_negative_usbi3c_initialize_device_table_config_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *target_device_table_buffer = NULL;
	int ret = -1;
	const int INTERRUPT_ENDPOINT = 3;
	const int NUMBER_OF_DEVICES = 3;
	const int DEVICE_CAPABILITY = 0b000000000111;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a successful bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	/* mock for getting the target device table */
	target_device_table_buffer = mock_get_target_device_table(&fake_handle, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);
	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(&fake_handle,
												INITIAL_TARGET_ADDRESS_POOL,
												NUMBER_OF_DEVICES,
												DEVICE_CAPABILITY,
												DEFAULT_MAX_IBI_PAYLOAD_SIZE,
												RETURN_FAILURE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);

	free(buffer);
	free(configure_buffer);
	free(notification);
	free(target_device_table_buffer);
}

/* Negative test to validate the I3C controller handle second get target device table request failure gracefully */
static void test_negative_usbi3c_initialize_device_second_table_request_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *target_device_table_buffer = NULL;
	uint8_t *target_device_table_buffer2 = NULL;
	int ret = -1;
	const int INTERRUPT_ENDPOINT = 3;
	const int NUMBER_OF_DEVICES = 3;
	const int DEVICE_CAPABILITY = 0b000000000111;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a successful bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	/* mock for getting the target device table */
	target_device_table_buffer = mock_get_target_device_table(&fake_handle, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);
	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(&fake_handle,
												INITIAL_TARGET_ADDRESS_POOL,
												NUMBER_OF_DEVICES,
												DEVICE_CAPABILITY,
												DEFAULT_MAX_IBI_PAYLOAD_SIZE,
												RETURN_SUCCESS);

	// create target device table buffer
	target_device_table_buffer2 = mock_get_target_device_table(&fake_handle,
								   NUMBER_OF_DEVICES,
								   TARGET_CONFIG,
								   TARGET_CAPABILITY,
								   RETURN_FAILURE);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);

	free(buffer);
	free(configure_buffer);
	free(notification);
	free(target_device_table_buffer);
	free(target_device_table_buffer2);
}

/* Test to validate the I3C controller can be initialized successfully */
static void test_usbi3c_initialize_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *target_device_table_buffer = NULL;
	uint8_t *target_device_table_buffer2 = NULL;
	int ret = -1;
	const int INTERRUPT_ENDPOINT = 3;
	const int NUMBER_OF_DEVICES = 3;
	const int DEVICE_CAPABILITY = 0b000000000111;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
					 DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a successful bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	/* mock for getting the target device table */
	target_device_table_buffer = mock_get_target_device_table(&fake_handle, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);
	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(&fake_handle,
												INITIAL_TARGET_ADDRESS_POOL,
												NUMBER_OF_DEVICES,
												DEVICE_CAPABILITY,
												DEFAULT_MAX_IBI_PAYLOAD_SIZE,
												RETURN_SUCCESS);

	// create target device table buffer
	target_device_table_buffer2 = mock_get_target_device_table(&fake_handle,
								   NUMBER_OF_DEVICES,
								   TARGET_CONFIG,
								   TARGET_CAPABILITY,
								   RETURN_SUCCESS);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.active_i3c_controller, TRUE);

	free(buffer);
	free(configure_buffer);
	free(notification);
	free(target_device_table_buffer);
	free(target_device_table_buffer2);
}

/* Test to validate the I3C controller can be initialized successfully even when it doesn't have capability data */
static void test_usbi3c_initialize_device_with_no_capability_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *target_device_table_buffer = NULL;
	uint8_t *target_device_table_buffer2 = NULL;
	int ret = -1;
	const int NOT_APPLICABLE = 0;
	const int INTERRUPT_ENDPOINT = 3;
	const int NUMBER_OF_DEVICES = 3;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;

	/* we are going to mock a control transfer for GET_I3C_CAPABILITY, when an I3C Device
	 * does not contain I3C Capability data, the I3C Function shall return the I3C_CAPABILITY_HEADER
	 * of I3C Capability data structure with field Total Length set to 4 bytes, fields Device Role and
	 * Data Type set to 0h, and the Error Code field set to FFh */
	unsigned char *buffer = NULL;
	buffer = mock_get_i3c_capability(&fake_handle, DEVICE_DOES_NOT_CONTAIN_CAPABILITY_DATA, NOT_APPLICABLE, NOT_APPLICABLE,
					 NOT_APPLICABLE, NOT_APPLICABLE, RETURN_SUCCESS);
	assert_non_null(buffer);

	/* mock a successful bulk response buffer init */
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	/* mock libusb_submit_transfer to return a success code every time is called */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(&fake_handle, ENTER_DYNAMIC_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	/* create a fake notification for a successful bus initialization that will be received async */
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(INTERRUPT_ENDPOINT, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(INTERRUPT_ENDPOINT, RETURN_SUCCESS);

	/* mock for getting the target device table */
	target_device_table_buffer = mock_get_target_device_table(&fake_handle, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);

	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(&fake_handle,
												INITIAL_TARGET_ADDRESS_POOL,
												NUMBER_OF_DEVICES,
												0,
												0,
												RETURN_SUCCESS);

	// create target device table buffer
	target_device_table_buffer2 = mock_get_target_device_table(&fake_handle,
								   NUMBER_OF_DEVICES,
								   TARGET_CONFIG,
								   TARGET_CAPABILITY,
								   RETURN_SUCCESS);

	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, 0);

	/* since the device does not contain capability data, a role of I3C controller
	 * with no knowledge of target devices on the bus should have been assumed */
	assert_int_equal(deps->usbi3c_dev->device_info->device_role, USBI3C_PRIMARY_CONTROLLER_ROLE);
	assert_int_equal(deps->usbi3c_dev->device_info->data_type, NO_STATIC_DATA);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.active_i3c_controller, TRUE);

	free(buffer);
	free(configure_buffer);
	free(notification);
	free(target_device_table_buffer);
	free(target_device_table_buffer2);
}

int main(void)
{

	/* Unit tests for the usbi3c_initialize_device() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_get_capability_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_error_getting_capability_data, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_response_buffer_init_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_bulk_transfer_polling_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_interrupt_init_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_i3c_bus_init_error, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_i3c_bus_init_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_target_device_table_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_initialize_device_table_config_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_initialize_device_second_table_request_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_initialize_device, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_initialize_device_with_no_capability_data, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
