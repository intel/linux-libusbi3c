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

#include "helpers.h"
#include "mocks.h"
#include "target_device_table_i.h"

const uint8_t OLD_ADDRESS = INITIAL_TARGET_ADDRESS_POOL;
const uint8_t NEW_ADDRESS = INITIAL_TARGET_ADDRESS_POOL + 10;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
	struct device_info *device_info;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

	/* keep a copy of this to recover it */
	deps->device_info = deps->usbi3c_dev->device_info;

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	deps->usbi3c_dev->device_info = deps->device_info;
	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

static void address_change_failed_cb(uint8_t old_address, uint8_t new_address, enum usbi3c_address_change_status request_status, void *data)
{
	assert_int_equal(old_address, OLD_ADDRESS);
	assert_int_equal(new_address, NEW_ADDRESS);
	assert_int_equal(request_status, USBI3C_ADDRESS_CHANGE_FAILED);
	assert_int_equal(*(int *)data, FALSE);

	int *callback_executed = (int *)data;
	*callback_executed = TRUE;

	return;
}

static void address_change_succeeded_cb(uint8_t old_address, uint8_t new_address, enum usbi3c_address_change_status request_status, void *data)
{
	assert_int_equal(old_address, OLD_ADDRESS);
	assert_int_equal(new_address, NEW_ADDRESS);
	assert_int_equal(request_status, USBI3C_ADDRESS_CHANGE_SUCCEEDED);
	assert_int_equal(*(int *)data, FALSE);

	int *callback_executed = (int *)data;
	*callback_executed = TRUE;

	return;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_change_i3c_device_address(NULL, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an address with an empty device gracefully */
static void test_negative_empty_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int EMPTY_ADDRESS = 0;

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, EMPTY_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device with unknown capabilities gracefully */
static void test_negative_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to use it with a non active controller gracefully */
static void test_negative_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles trying to use an already assigned address gracefully */
static void test_negative_new_address_already_taken(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, OLD_ADDRESS + 1, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a failure in libusb gracefully */
static void test_negative_change_address_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int ret = 0;

	/* create a fake control request that resembles the one we expect */
	buffer = mock_change_dynamic_address(OLD_ADDRESS, NEW_ADDRESS, RETURN_FAILURE);

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, NULL);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Negative test to verify the function handles a failure in libusb getting the change result gracefully */
static void test_negative_change_result_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	unsigned char *address_change_buffer = NULL;
	unsigned char *notification_buffer = NULL;
	int callback_flag = FALSE;
	int buffer_size = 0;
	int ret = 0;

	/* create a fake control request that resembles the one we expect */
	address_change_buffer = mock_change_dynamic_address(OLD_ADDRESS, NEW_ADDRESS, RETURN_SUCCESS);

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, &callback_flag);
	assert_int_equal(ret, 0);

	/* we'll force libusb to fail to submit an async control transfer */
	mock_libusb_submit_transfer(LIBUSB_ERROR_BUSY);

	/* the I3C function should notify the host when the address change was processed
	 * so let's simulate an "Address Change" notification coming from the I3C function */
	buffer_size = helper_create_notification_buffer(&notification_buffer, NOTIFICATION_ADDRESS_CHANGE_STATUS, ALL_ADDRESS_CHANGE_SUCCEEDED);
	helper_trigger_notification(notification_buffer, buffer_size);

	/* verify the address of the device was NOT changed */
	device = table_get_device(deps->usbi3c_dev->target_device_table, OLD_ADDRESS);
	assert_non_null(device);
	device = table_get_device(deps->usbi3c_dev->target_device_table, NEW_ADDRESS);
	assert_null(device);

	free(address_change_buffer);
	free(notification_buffer);
}

/* Negative test to verify the function handles a failure from the I3C device changing the address gracefully */
static void test_negative_change_address_failed(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	unsigned char *address_change_buffer = NULL;
	unsigned char *notification_buffer = NULL;
	unsigned char *change_result_buffer = NULL;
	int buffer_size = 0;
	int ret = 0;
	int callback_executed = FALSE;

	/* create a fake control request that resembles the one we expect */
	address_change_buffer = mock_change_dynamic_address(OLD_ADDRESS, NEW_ADDRESS, RETURN_SUCCESS);

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_failed_cb, &callback_executed);
	assert_int_equal(ret, 0);

	/* the I3C function should notify the host when the address change was processed
	 * so let's simulate an "Address Change" notification coming from the I3C function */
	buffer_size = helper_create_notification_buffer(&notification_buffer, NOTIFICATION_ADDRESS_CHANGE_STATUS, SOME_ADDRESS_CHANGE_FAILED);
	helper_trigger_notification(notification_buffer, buffer_size);

	/* the notification handler is going to execute a GET_ADDRESS_CHANGE_RESULT request
	 * asynchronously, so we need to mock that event as well */
	mock_libusb_submit_transfer(LIBUSB_SUCCESS);
	buffer_size = helper_create_address_change_result_buffer(&change_result_buffer, OLD_ADDRESS, NEW_ADDRESS, 1, 1);
	helper_trigger_control_transfer(change_result_buffer, buffer_size);

	/* make sure the address change callback function was executed, but was passed the correct status */
	assert_int_equal(callback_executed, TRUE);

	/* verify the address of the device was NOT changed */
	device = table_get_device(deps->usbi3c_dev->target_device_table, OLD_ADDRESS);
	assert_non_null(device);
	device = table_get_device(deps->usbi3c_dev->target_device_table, NEW_ADDRESS);
	assert_null(device);

	free(address_change_buffer);
	free(notification_buffer);
	free(change_result_buffer);
}

/* Test to verify the dynamic address of a target device can be changed */
static void test_usbi3c_change_dynamic_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	struct address_change_request *request = NULL;
	unsigned char *address_change_buffer = NULL;
	unsigned char *notification_buffer = NULL;
	unsigned char *change_result_buffer = NULL;
	int buffer_size = 0;
	int callback_executed = FALSE;
	int ret = 0;

	/* create a fake control request that resembles the one we expect */
	address_change_buffer = mock_change_dynamic_address(OLD_ADDRESS, NEW_ADDRESS, RETURN_SUCCESS);

	/* let's add some dummy requests to the address change request tracker */
	request = (struct address_change_request *)malloc_or_die(sizeof(struct address_change_request));
	request->request_id = (OLD_ADDRESS << 8) + NEW_ADDRESS - 10;
	request->on_address_change_cb = NULL;
	request->user_data = NULL;
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	deps->usbi3c_dev->target_device_table->address_change_tracker = list_append(deps->usbi3c_dev->target_device_table->address_change_tracker, request);
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);
	request = (struct address_change_request *)malloc_or_die(sizeof(struct address_change_request));
	request->request_id = (OLD_ADDRESS << 8) + NEW_ADDRESS + 10;
	request->on_address_change_cb = NULL;
	request->user_data = NULL;
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	deps->usbi3c_dev->target_device_table->address_change_tracker = list_append(deps->usbi3c_dev->target_device_table->address_change_tracker, request);
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, address_change_succeeded_cb, &callback_executed);
	assert_int_equal(ret, 0);

	/* the I3C function should notify the host when the address change was processed
	 * so let's simulate an "Address Change" notification coming from the I3C function */
	buffer_size = helper_create_notification_buffer(&notification_buffer, NOTIFICATION_ADDRESS_CHANGE_STATUS, ALL_ADDRESS_CHANGE_SUCCEEDED);
	helper_trigger_notification(notification_buffer, buffer_size);

	/* the notification handler is going to execute a GET_ADDRESS_CHANGE_RESULT request
	 * asynchronously, so we need to mock that event as well */
	mock_libusb_submit_transfer(LIBUSB_SUCCESS);
	buffer_size = helper_create_address_change_result_buffer(&change_result_buffer, OLD_ADDRESS, NEW_ADDRESS, 1, USBI3C_SUCCEEDED);
	helper_trigger_control_transfer(change_result_buffer, buffer_size);

	/* make sure the address change callback function was executed */
	assert_int_equal(callback_executed, TRUE);

	/* verify the address of the device was changed */
	device = table_get_device(deps->usbi3c_dev->target_device_table, OLD_ADDRESS);
	assert_null(device);
	device = table_get_device(deps->usbi3c_dev->target_device_table, NEW_ADDRESS);
	assert_non_null(device);

	free(address_change_buffer);
	free(notification_buffer);
	free(change_result_buffer);
}

/* Test to verify the dynamic address of a target device can be changed without passing a callback */
static void test_usbi3c_change_dynamic_address_without_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	unsigned char *address_change_buffer = NULL;
	unsigned char *notification_buffer = NULL;
	unsigned char *change_result_buffer = NULL;
	int buffer_size = 0;
	int ret = 0;

	/* create a fake control request that resembles the one we expect */
	address_change_buffer = mock_change_dynamic_address(OLD_ADDRESS, NEW_ADDRESS, RETURN_SUCCESS);

	ret = usbi3c_change_i3c_device_address(deps->usbi3c_dev, OLD_ADDRESS, NEW_ADDRESS, NULL, NULL);
	assert_int_equal(ret, 0);

	/* the I3C function should notify the host when the address change was processed
	 * so let's simulate an "Address Change" notification coming from the I3C function */
	buffer_size = helper_create_notification_buffer(&notification_buffer, NOTIFICATION_ADDRESS_CHANGE_STATUS, ALL_ADDRESS_CHANGE_SUCCEEDED);
	helper_trigger_notification(notification_buffer, buffer_size);

	/* the notification handler is going to execute a GET_ADDRESS_CHANGE_RESULT request
	 * asynchronously, so we need to mock that event as well */
	mock_libusb_submit_transfer(LIBUSB_SUCCESS);
	buffer_size = helper_create_address_change_result_buffer(&change_result_buffer, OLD_ADDRESS, NEW_ADDRESS, 1, USBI3C_SUCCEEDED);
	helper_trigger_control_transfer(change_result_buffer, buffer_size);

	/* verify the address of the device was changed */
	device = table_get_device(deps->usbi3c_dev->target_device_table, OLD_ADDRESS);
	assert_null(device);
	device = table_get_device(deps->usbi3c_dev->target_device_table, NEW_ADDRESS);
	assert_non_null(device);

	free(address_change_buffer);
	free(notification_buffer);
	free(change_result_buffer);
}

int main(void)
{

	/* Unit tests for the usbi3c_change_i3c_device_address() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_empty_address, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_capabilities, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_not_active_controller, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_new_address_already_taken, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_change_address_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_change_result_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_change_address_failed, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_change_dynamic_address, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_change_dynamic_address_without_callback, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
