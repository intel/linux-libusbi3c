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

#include "usb_i.h"

struct libusb_device {
	int fake_device_member;
};

struct test_deps {
	struct usb_context *usb_ctx;
	struct usb_device *usb_dev;
};

static int setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usb_device **usb_devices;
	mock_libusb_init(NULL, RETURN_SUCCESS);
	usb_context_init(&deps->usb_ctx);

	struct libusb_device libusb_device = { .fake_device_member = 1 };
	struct libusb_device *libusb_devices[3];
	libusb_devices[0] = &libusb_device;
	libusb_devices[2] = NULL;

	mock_libusb_get_device_list(libusb_devices, 1);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&libusb_device);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int num_dev = usb_find_devices(deps->usb_ctx, NULL, &usb_devices);
	assert_int_equal(num_dev, 1);
	assert_non_null(usb_devices);
	deps->usb_dev = usb_device_ref(usb_devices[0]);
	*state = deps;
	usb_free_devices(usb_devices, num_dev);
	return 0;
}

static int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	if (usb_device_is_initialized(deps->usb_dev)) {
		mock_libusb_release_interface(NULL, 0);
		mock_libusb_close();
	}
	usb_device_deinit(deps->usb_dev);
	usb_context_deinit(deps->usb_ctx);
	free(deps);
	return 0;
}

static void fake_init(struct usb_device *usb_dev, void *fake_handle)
{
	int ret = -1;
	mock_libusb_open(fake_handle, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(fake_handle, 1);
	mock_libusb_detach_kernel_driver(fake_handle, RETURN_SUCCESS);
	mock_libusb_claim_interface(fake_handle, RETURN_SUCCESS);
	ret = usb_device_init(usb_dev);
	assert_int_equal(ret, RETURN_SUCCESS);
}

void callback(void *user_context, unsigned char *buffer, uint16_t buffer_size)
{
	int *counter = (int *)user_context;
	for (int i = 0; i < buffer_size; i++) {
		*counter += buffer[i];
	}
}

/* Test input control transfer async for an uninitialized device */
static void test_negative_input_control_async_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usb_input_control_transfer_async(deps->usb_dev, 0, 0, 0, callback, NULL);
	assert_int_not_equal(ret, RETURN_SUCCESS);
}

/* Test input control transfer async handling transfer failures */
static void test_negative_input_control_async_not_completed(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int counter = 0;

	fake_init(deps->usb_dev, NULL);

	mock_usb_input_control_transfer_async(RETURN_SUCCESS);
	mock_libusb_submit_transfer(RETURN_SUCCESS);
	int res = usb_input_control_transfer_async(deps->usb_dev, 0, 0, 0, callback, &counter);
	assert_int_equal(res, RETURN_SUCCESS);

	fake_transfer_set_transaction_status(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
					     LIBUSB_TRANSFER_TIMED_OUT);
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, NULL, 0);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(usb_get_errno(deps->usb_dev), LIBUSB_TRANSFER_TIMED_OUT);
}

/* Test input control transfer async handling libusb failure */
static void test_negative_input_control_async_fail_submit(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int counter = 0;

	fake_init(deps->usb_dev, NULL);

	mock_libusb_submit_transfer(LIBUSB_ERROR_NO_DEVICE);
	int res = usb_input_control_transfer_async(deps->usb_dev, 0, 0, 0, callback, &counter);
	assert_int_equal(res, LIBUSB_ERROR_NO_DEVICE);
}

/* Test input control transfer async succesful */
void test_input_control_async(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char buffer[15] = { 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5 };
	int counter = 0;

	fake_init(deps->usb_dev, NULL);

	mock_libusb_submit_transfer(RETURN_SUCCESS);
	int res = usb_input_control_transfer_async(deps->usb_dev, 0, 0, 0, callback, &counter);
	assert_int_equal(res, RETURN_SUCCESS);

	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, buffer, 15);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(counter, 55);
}

unsigned char data[] = "Arbitrary test data";
/* test output control transfer async for an uninitialized device */
static void test_negative_output_control_async_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usb_output_control_transfer_async(deps->usb_dev, 0, 0, 0, data, (uint16_t)sizeof(data), callback, NULL);
	assert_int_not_equal(ret, RETURN_SUCCESS);
}

/* Test output control transfer async handling transfer failures */
static void test_negative_output_control_async_not_completed(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	fake_init(deps->usb_dev, NULL);

	mock_libusb_submit_transfer(RETURN_SUCCESS);
	ret = usb_output_control_transfer_async(deps->usb_dev, 0, 0, 0, data, (uint16_t)sizeof(data), callback, NULL);
	assert_int_equal(ret, RETURN_SUCCESS);

	fake_transfer_set_transaction_status(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
					     LIBUSB_TRANSFER_TIMED_OUT);
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, NULL, 0);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(usb_get_errno(deps->usb_dev), LIBUSB_TRANSFER_TIMED_OUT);
}

/* Test output control transfer async handling libusb failure */
static void test_negative_output_control_async_fail_submit(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	fake_init(deps->usb_dev, NULL);

	mock_libusb_submit_transfer(LIBUSB_ERROR_NO_DEVICE);
	int ret = usb_output_control_transfer_async(deps->usb_dev, 0, 0, 0, data, (uint16_t)sizeof(data), callback, NULL);
	assert_int_equal(ret, LIBUSB_ERROR_NO_DEVICE);
}

void output_callback(void *user_context, unsigned char *buffer, uint16_t buffer_size)
{
	int *called = (int *)user_context;
	*called = 1;
}

/* Test output control transfer async succesful */
void test_output_control_async(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int called = 0;

	fake_init(deps->usb_dev, NULL);

	mock_libusb_submit_transfer(RETURN_SUCCESS);
	int res = usb_output_control_transfer_async(deps->usb_dev, 0, 0, 0, data, (uint16_t)sizeof(data), output_callback, &called);
	assert_int_equal(res, RETURN_SUCCESS);

	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, NULL, 0);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(called, 1);
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_input_control_async_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_input_control_async_not_completed, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_input_control_async_fail_submit, setup, teardown),
		cmocka_unit_test_setup_teardown(test_input_control_async, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_control_async_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_control_async_not_completed, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_control_async_fail_submit, setup, teardown),
		cmocka_unit_test_setup_teardown(test_output_control_async, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
