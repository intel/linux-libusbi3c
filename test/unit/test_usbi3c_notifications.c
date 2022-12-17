/***************************************************************************
  USBI3C  -  Library to talk to I3C devices via USB.
  -------------------
  copyright            : (C) 2022 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation.             *
 *                                                                         *
 ***************************************************************************/

#include <pthread.h>

#include "helpers.h"
#include "mocks.h"
#include "target_device_table_i.h"

static int fake_handle = 1;
static pthread_mutex_t data_mutex;
#define WRONG_NOTIFICATION_TYPE 10
#define WRONG_NOTIFICATION_CODE 11

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

static void on_bus_error(uint8_t error_code, void *data)
{
	pthread_mutex_lock(&data_mutex);
	*((uint8_t *)data) = error_code;
	pthread_mutex_unlock(&data_mutex);
}

/* test call a notification that is not supported */
static void test_not_sopported_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	/* this kind of notification is supported by the active controller */
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

	// create fake notification to receive and trigger it
	buffer_size = helper_create_notification_buffer(&buffer, WRONG_NOTIFICATION_TYPE, WRONG_NOTIFICATION_CODE);
	helper_trigger_notification(buffer, buffer_size);

	// check on_bus_error function has been called
	free(buffer);
}

/* Test to validate that user callback function is called when an bus error notification is triggered */
static void test_bus_error_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	uint8_t error_code = 0;
	pthread_mutex_init(&data_mutex, NULL);

	/* this kind of notification is supported by the active controller */
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

	// assign callback function that will be called in case of bus error
	usbi3c_on_bus_error(deps->usbi3c_dev, on_bus_error, &error_code);

	// create fake notification to receive and trigger it
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_I3C_BUS_ERROR, NEW_I3C_CONTROLLER_FAILS_TO_DRIVE_I3C_BUS);
	helper_trigger_notification(buffer, buffer_size);

	// check on_bus_error function has been called
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(error_code, NEW_I3C_CONTROLLER_FAILS_TO_DRIVE_I3C_BUS);
	pthread_mutex_unlock(&data_mutex);

	free(buffer);
	pthread_mutex_destroy(&data_mutex);
}

static void on_controller_event_cb(enum usbi3c_controller_event_code event_code, void *data)
{
	pthread_mutex_lock(&data_mutex);
	*((uint8_t *)data) = event_code;
	pthread_mutex_unlock(&data_mutex);
}

/* Test to validate that user provided callback function is called when an event is received from the active I3C controller */
static void test_i3c_controller_event_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	uint8_t event_code = 0;
	pthread_mutex_init(&data_mutex, NULL);

	/* this kind of notification is only supported by the target device */
	helper_initialize_target_device(deps->usbi3c_dev, &fake_handle);

	/* assign a callback function that will be run after receiving the notification */
	usbi3c_on_controller_event(deps->usbi3c_dev, on_controller_event_cb, &event_code);

	/* create a fake notification to receive, and trigger it */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_ACTIVE_I3C_CONTROLLER_EVENT, RECEIVED_READ_REQUEST);
	helper_trigger_notification(buffer, buffer_size);

	/* make sure the callback was called */
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(event_code, RECEIVED_READ_REQUEST);
	pthread_mutex_unlock(&data_mutex);

	free(buffer);
	pthread_mutex_destroy(&data_mutex);
}

/* Test to validate that upon receiving a an I3C controller notification, if the user did not provided a callback
 * function, everything works normally, and no errors are generated */
static void test_i3c_controller_event_notification_without_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	/* this kind of notification is only supported by the target device */
	helper_initialize_target_device(deps->usbi3c_dev, &fake_handle);

	/* create a fake notification to receive, and trigger it */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_ACTIVE_I3C_CONTROLLER_EVENT, RECEIVED_READ_REQUEST);
	helper_trigger_notification(buffer, buffer_size);

	free(buffer);
}

static void test_change_address_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	struct target_device *device = NULL;
	pthread_mutex_init(&data_mutex, NULL);

	/* this kind of notification is supported by the active controller */
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

	// create notification buffer
	buffer_size = helper_create_notification_buffer(&buffer,
							NOTIFICATION_ADDRESS_CHANGE_STATUS,
							ALL_ADDRESS_CHANGE_SUCCEEDED);
	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 5;
	uint8_t *change_result_buffer = NULL;
	uint16_t change_result_buffer_size = 0;

	/* the current address of the device is TARGET_ADDRESS_1 */
	pthread_mutex_lock(&data_mutex);
	device = table_get_device(deps->usbi3c_dev->target_device_table, TARGET_ADDRESS_1);
	assert_non_null(device);

	/* the new address of the device TARGET_ADDRESS_2 is available */
	device = table_get_device(deps->usbi3c_dev->target_device_table, TARGET_ADDRESS_2);
	assert_null(device);
	pthread_mutex_unlock(&data_mutex);

	// create control transfer buffer
	change_result_buffer_size = helper_create_address_change_result_buffer(&change_result_buffer,
									       TARGET_ADDRESS_1,
									       TARGET_ADDRESS_2,
									       1,
									       RETURN_SUCCESS);

	// trigger notification
	helper_trigger_notification(buffer, buffer_size);

	// trigger control transfer response
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, change_result_buffer, change_result_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* verify the address of the device was changed */
	pthread_mutex_lock(&data_mutex);
	device = table_get_device(deps->usbi3c_dev->target_device_table, TARGET_ADDRESS_1);
	assert_null(device);
	device = table_get_device(deps->usbi3c_dev->target_device_table, TARGET_ADDRESS_2);
	assert_non_null(device);
	pthread_mutex_unlock(&data_mutex);

	pthread_mutex_destroy(&data_mutex);
	free(buffer);
	free(change_result_buffer);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_not_sopported_notification, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_bus_error_notification, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_i3c_controller_event_notification, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_i3c_controller_event_notification_without_callback, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_change_address_notification, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
