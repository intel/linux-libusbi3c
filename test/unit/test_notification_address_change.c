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

static pthread_mutex_t data_mutex;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

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

static void on_hotjoin_cb(uint8_t hot_joined_address, void *data)
{
	int *address = (int *)data;
	pthread_mutex_lock(&data_mutex);
	*address = hot_joined_address;
	pthread_mutex_unlock(&data_mutex);
}

/* Negative test to verify that the function handles receiving an address change failed notification gracefully */
static void test_negative_hotjoin_notification_address_assignment_failed(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	size_t buffer_size = 0;
	int address = 0;
	pthread_mutex_init(&data_mutex, NULL);

	usbi3c_on_hotjoin(deps->usbi3c_dev, on_hotjoin_cb, &address);

	/* create notification buffer */
	buffer_size = helper_create_notification_buffer(&buffer,
							NOTIFICATION_ADDRESS_CHANGE_STATUS,
							HOTJOIN_ADDRESS_ASSIGNMENT_FAILED);

	/* trigger notification */
	helper_trigger_notification(buffer, buffer_size);

	/* By default, the helper_initialize_controller() function will add 3 devices to the bus
	 * with addresses 100, 101, and 102. Verify no devices were added to the table */
	int count = 0;
	for (int i = 0; i < 255; i++) {
		struct target_device *device = table_get_device(deps->usbi3c_dev->target_device_table, i);
		if (device != NULL) {
			count++;
		}
	}
	assert_int_equal(count, 3);

	/* the callback shouldn't have been called */
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(address, 0);
	pthread_mutex_unlock(&data_mutex);

	pthread_mutex_destroy(&data_mutex);
	free(buffer);
}

/* Negative test to validate that the function handles an error in libusb getting the table after a hot-join gracefully */
static void test_negative_hotjoin_notification_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	size_t buffer_size;
	int address = 0;
	pthread_mutex_init(&data_mutex, NULL);

	usbi3c_on_hotjoin(deps->usbi3c_dev, on_hotjoin_cb, &address);

	/* create notification buffer */
	buffer_size = helper_create_notification_buffer(&buffer,
							NOTIFICATION_ADDRESS_CHANGE_STATUS,
							HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED);

	/* force a libusb failure getting the table async */
	mock_usb_output_control_transfer_async(RETURN_FAILURE);

	/* trigger notification */
	helper_trigger_notification(buffer, buffer_size);

	/* By default, the helper_initialize_controller() function will add 3 devices to the bus
	 * with addresses 100, 101, and 102. Since there was a failure updating the target device
	 * table, only 3 devices should still exist in the table */
	int count = 0;
	for (int i = 0; i < 255; i++) {
		struct target_device *device = table_get_device(deps->usbi3c_dev->target_device_table, i);
		if (device != NULL) {
			count++;
		}
	}
	assert_int_equal(count, 3);

	/* the callback function shouldn't have run */
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(address, 0);
	pthread_mutex_unlock(&data_mutex);

	pthread_mutex_destroy(&data_mutex);
	free(buffer);
}

/* Test to validate that when an "Address Change" notification is received, the target device
 * table is updated, and the user provided callback is run */
static void test_hotjoin_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	size_t buffer_size;
	int address = 0;
	pthread_mutex_init(&data_mutex, NULL);

	usbi3c_on_hotjoin(deps->usbi3c_dev, on_hotjoin_cb, &address);

	/* create notification buffer */
	buffer_size = helper_create_notification_buffer(&buffer,
							NOTIFICATION_ADDRESS_CHANGE_STATUS,
							HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED);

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL + 3;
	const int DEVICES_IN_BUS = 4;
	const int TARGET_CAPABILITY = 0b1000000000;
	const int TARGET_CONFIG = 0b00000000;

	/* create target device table buffer */
	unsigned char *hotjoin_buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	int hotjoin_buffer_size = sizeof(struct target_device_table_header) + (sizeof(struct target_device_table_entry) * 4);

	/* trigger notification */
	helper_trigger_notification(buffer, buffer_size);

	/* trigger control transfer response for get target device table request */
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, hotjoin_buffer, hotjoin_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* By default, the helper_initialize_controller() function will add 3 devices to the bus
	 * with addresses 100, 101, and 102. One more address should now exist result of the Hot-Join */
	int count = 0;
	for (int i = 0; i < 255; i++) {
		struct target_device *device = table_get_device(deps->usbi3c_dev->target_device_table, i);
		if (device != NULL) {
			count++;
		}
	}
	assert_int_equal(count, 4);

	/* the callback function should have run */
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(address, TARGET_ADDRESS_1);
	pthread_mutex_unlock(&data_mutex);

	pthread_mutex_destroy(&data_mutex);
	free(hotjoin_buffer);
	free(buffer);
}

/* Test to validate that when an "Address Change" notification is received, and there is no user provided
 * callback, the target device table is still updated */
static void test_hotjoin_notification_without_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	size_t buffer_size;
	int address = 0;
	pthread_mutex_init(&data_mutex, NULL);

	/* create notification buffer */
	buffer_size = helper_create_notification_buffer(&buffer,
							NOTIFICATION_ADDRESS_CHANGE_STATUS,
							HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED);

	const int DEVICES_IN_BUS = 4;
	const int TARGET_CAPABILITY = 0b1000000000;
	const int TARGET_CONFIG = 0b00000000;

	/* create target device table buffer */
	unsigned char *hotjoin_buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	int hotjoin_buffer_size = sizeof(struct target_device_table_header) + (sizeof(struct target_device_table_entry) * 4);

	/* trigger notification */
	helper_trigger_notification(buffer, buffer_size);

	/* trigger control transfer response for get target device table request */
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, hotjoin_buffer, hotjoin_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* By default, the helper_initialize_controller() function will add 3 devices to the bus
	 * with addresses 100, 101, and 102. One more address should now exist result of the Hot-Join */
	int count = 0;
	for (int i = 0; i < 255; i++) {
		struct target_device *device = table_get_device(deps->usbi3c_dev->target_device_table, i);
		if (device != NULL) {
			count++;
		}
	}
	assert_int_equal(count, 4);

	/* the callback function should not have run since it was not assigned */
	pthread_mutex_lock(&data_mutex);
	assert_int_equal(address, 0);
	pthread_mutex_unlock(&data_mutex);

	pthread_mutex_destroy(&data_mutex);
	free(hotjoin_buffer);
	free(buffer);
}

int main(void)
{
	/* Unit tests for the "address change" notification handling */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_hotjoin_notification_address_assignment_failed, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_hotjoin_notification_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_hotjoin_notification, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_hotjoin_notification_without_callback, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
