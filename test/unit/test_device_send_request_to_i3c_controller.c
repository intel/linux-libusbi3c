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

#include "target_device_table_i.h"

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* the device_send_request_to_i3c_controller() function will run properly only if the bulk transfer polling has
 * been initiated. This is commonly run during usbi3c_initialize_device(), but  we need to
 * initialize it manually for these tests */
static void initialize_bulk_transfer_polling(struct usbi3c_device *usbi3c_dev)
{
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);
	mock_usb_input_bulk_transfer_polling(RETURN_SUCCESS);

	buffer_size = usb_bulk_transfer_response_buffer_init(usbi3c_dev->usb_dev, &buffer);
	usb_input_bulk_transfer_polling(usbi3c_dev->usb_dev, buffer, buffer_size, bulk_transfer_get_response);
}

/* Negative test to validate that a missing context is handled gracefully */
static void test_negative_missing_context(void **state)
{
	int ret = 0;
	const int ADDRESS = 0x10;

	ret = device_send_request_to_i3c_controller(NULL, ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to validate that only the correct READ or WRITE direction is allowed */
static void test_negative_bad_direction_value(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int ADDRESS = 0x10;
	const int BAD_COMMAND_DIRECTION = 0x3;

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, ADDRESS, BAD_COMMAND_DIRECTION);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to validate that the command queue has to be empty to request the hot-join */
static void test_negative_command_queue_not_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int ADDRESS = 0x10;
	const int RANDOM_ADDRESS = 0xFF;

	/* enqueue a random command */
	usbi3c_enqueue_command(deps->usbi3c_dev,
			       RANDOM_ADDRESS,
			       USBI3C_WRITE,
			       USBI3C_TERMINATE_ON_ANY_ERROR,
			       0,
			       NULL,
			       NULL,
			       NULL);

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to validate that the function won't run unless the device has been initialized yet */
static void test_negative_device_not_init(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int ADDRESS = 0x10;

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to validate that a failure in libusb sending the request is handled gracefully */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *request_buffer = NULL;
	int request_buffer_size = 0;
	int buffer_available = 1000;
	int ret = 0;

	initialize_bulk_transfer_polling(deps->usbi3c_dev);

	/* the hot-join request is sent as a sync bulk request transfer,
	 * let's create the type of buffer we expect so we can compare it
	 * against the one generated by the library */
	request_buffer_size = helper_create_command_buffer(bulk_request_id,
							   &request_buffer,
							   HOT_JOIN_ADDRESS,
							   USBI3C_WRITE,
							   USBI3C_TERMINATE_ON_ANY_ERROR,
							   0,
							   NULL,
							   DEFAULT_TRANSFER_MODE,
							   DEFAULT_TRANSFER_RATE,
							   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* mock the USB related functions */
	mock_get_buffer_available(NULL, &buffer_available, RETURN_SUCCESS);
	mock_usb_output_bulk_transfer(request_buffer, request_buffer_size, LIBUSB_ERROR_BUSY);

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, HOT_JOIN_ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_FAILURE);

	free(request_buffer);
}

/* Negative test to validate that an unattempted hot-join command is handled gracefully */
static void test_negative_command_not_attempted(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *request_buffer = NULL;
	unsigned char *response_buffer = NULL;
	int request_buffer_size = 0;
	int response_buffer_size = 0;
	int buffer_available = 1000;
	int ret = 0;

	initialize_bulk_transfer_polling(deps->usbi3c_dev);

	/* the hot-join request is sent as a sync bulk request transfer,
	 * let's create the type of buffer we expect so we can compare it
	 * against the one generated by the library */
	request_buffer_size = helper_create_command_buffer(bulk_request_id,
							   &request_buffer,
							   HOT_JOIN_ADDRESS,
							   USBI3C_WRITE,
							   USBI3C_TERMINATE_ON_ANY_ERROR,
							   0,
							   NULL,
							   DEFAULT_TRANSFER_MODE,
							   DEFAULT_TRANSFER_RATE,
							   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* once the target device has succeeded or failed hot-joining a bus it
	 * informs the host about it using a bulk response transfer. Let's create
	 * the type of buffer we expect to receive so we mock a response from a device */
	struct usbi3c_response response = { 0 };
	response.attempted = USBI3C_COMMAND_NOT_ATTEMPTED;
	response.error_status = USBI3C_SUCCEEDED;
	response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	response.data_length = 0;
	response.data = NULL;
	response_buffer_size = helper_create_response_buffer(&response_buffer,
							     &response,
							     bulk_request_id);

	/* mock the USB related functions */
	mock_get_buffer_available(NULL, &buffer_available, RETURN_SUCCESS);
	mock_usb_output_bulk_transfer(request_buffer, request_buffer_size, RETURN_SUCCESS);
	mock_usb_wait_for_next_event(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response_buffer, response_buffer_size, RETURN_SUCCESS);

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, HOT_JOIN_ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_FAILURE);

	free(request_buffer);
	free(response_buffer);
}

static void test_bus_request_hotjoin(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *request_buffer = NULL;
	unsigned char *response_buffer = NULL;
	int request_buffer_size = 0;
	int response_buffer_size = 0;
	int buffer_available = 1000;
	int ret = -1;

	initialize_bulk_transfer_polling(deps->usbi3c_dev);

	/* the hot-join request is sent as a sync bulk request transfer,
	 * let's create the type of buffer we expect so we can compare it
	 * against the one generated by the library */
	request_buffer_size = helper_create_command_buffer(bulk_request_id,
							   &request_buffer,
							   HOT_JOIN_ADDRESS,
							   USBI3C_WRITE,
							   USBI3C_TERMINATE_ON_ANY_ERROR,
							   0,
							   NULL,
							   DEFAULT_TRANSFER_MODE,
							   DEFAULT_TRANSFER_RATE,
							   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* once the target device has succeeded or failed hot-joining a bus it
	 * informs the host about it using a bulk response transfer. Let's create
	 * the type of buffer we expect to receive so we mock a response from a device */
	struct usbi3c_response response = { 0 };
	response.attempted = USBI3C_COMMAND_ATTEMPTED;
	response.error_status = USBI3C_SUCCEEDED;
	response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	response.data_length = 0;
	response.data = NULL;
	response_buffer_size = helper_create_response_buffer(&response_buffer,
							     &response,
							     bulk_request_id);

	/* mock the USB related functions */
	mock_get_buffer_available(NULL, &buffer_available, RETURN_SUCCESS);
	mock_usb_output_bulk_transfer(request_buffer, request_buffer_size, RETURN_SUCCESS);
	mock_usb_wait_for_next_event(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response_buffer, response_buffer_size, RETURN_SUCCESS);

	ret = device_send_request_to_i3c_controller(deps->usbi3c_dev, HOT_JOIN_ADDRESS, USBI3C_WRITE);
	assert_int_equal(ret, RETURN_SUCCESS);

	free(request_buffer);
	free(response_buffer);
}

int main(void)
{
	/* Unit tests for the device_send_request_to_i3c_controller() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_bad_direction_value, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_command_queue_not_empty, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_device_not_init, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_command_not_attempted, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_bus_request_hotjoin, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
