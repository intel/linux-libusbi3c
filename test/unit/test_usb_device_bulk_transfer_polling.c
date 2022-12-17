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

void transfer_dispatcher_cb(void *context, unsigned char *buffer, uint32_t buffer_size)
{
	int *called = (int *)context;
	*called = 1;
}

/* test USB input transfer polling for uninitialized device */
static void test_negative_uninitialized_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	unsigned char *buffer = NULL;
	int buffer_size = 100;

	/* create an arbitrary buffer of 100 bytes */
	buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, RETURN_FAILURE);

	free(buffer);
	assert_false(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

/* Negative test to validate that the function handles a missing buffer gracefully */
static void test_negative_missing_buffer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	fake_init(deps->usb_dev, NULL);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

/* Negative test to validate that the function require users to add a callback function */
static void test_negative_missing_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	unsigned char *buffer = NULL;
	int buffer_size = 100;

	fake_init(deps->usb_dev, NULL);

	/* create an arbitrary buffer of 100 bytes, this buffer will be attached to the
	 * usb session and freed during usb_deinit (during teardown), no need to manually free it */
	buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      NULL);
	assert_int_equal(ret, -1);
	assert_false(usb_input_bulk_transfer_polling_status(deps->usb_dev));
	free(buffer);
}

/* Negative test to validate that the function handles a libusb error gracefully */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	unsigned char *buffer = NULL;
	int called = 0;
	int buffer_size = 100;

	fake_init(deps->usb_dev, NULL);

	usb_set_bulk_transfer_context(deps->usb_dev, &called);

	/* create an arbitrary buffer of 100 bytes, this buffer will be attached to the
	 * usb session and freed during usb_deinit (during teardown), no need to manually free it */
	buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	/* mock libusb to fake an error */
	mock_libusb_submit_transfer(LIBUSB_ERROR_IO);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, LIBUSB_ERROR_IO);
	assert_false(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

/* Negative test to validate that a failure in the bulk transfer polling mechanism is handled gracefully */
static void test_negative_transfer_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 100;
	int ret = -1;
	int called = 0;
	unsigned char response[] = "Test response!";

	fake_init(deps->usb_dev, NULL);

	/* create an arbitrary buffer of 100 bytes, this buffer will be attached to the
	 * usb session and freed during usb_deinit (during teardown), no need to manually free it */
	buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	/* mock libusb to fake an success transfer submit */
	mock_libusb_submit_transfer(LIBUSB_SUCCESS);

	usb_set_bulk_transfer_context(deps->usb_dev, &called);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, LIBUSB_SUCCESS);

	/* simulate a response received from the I3c function */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response, sizeof(response));
	fake_transfer_set_transaction_status(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, LIBUSB_TRANSFER_STALL);
	fake_transfer_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX);

	/* the transfer dispatcher callback should not have been run
	 * and an error should have been logged */
	assert_int_equal(called, 0);
	assert_int_equal(usb_get_errno(deps->usb_dev), LIBUSB_TRANSFER_STALL);
	assert_true(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

/* Test to validate bulk transfer polling handle failures after its callback is called once */
static void test_negative_transfer_failure_after_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 100;
	int ret = -1;
	int called = 0;
	unsigned char response[] = "Test response!";

	fake_init(deps->usb_dev, NULL);

	usb_set_bulk_transfer_context(deps->usb_dev, &called);

	/* create an arbitrary buffer of 100 bytes, this buffer will be attached to the
	 * usb session and freed during usb_deinit (during teardown), no need to manually free it */
	buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	/* mock libusb to fake an success transfer submit and fail in the second one*/
	mock_libusb_submit_transfer(1);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, 0);

	/* simulate a response received from the I3c function */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response, sizeof(response));
	fake_transfer_set_transaction_status(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, LIBUSB_TRANSFER_COMPLETED);
	fake_transfer_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX);

	/* the transfer dispatcher callback should have been run */
	assert_int_equal(called, 1);
	assert_int_equal(usb_get_errno(deps->usb_dev), RETURN_FAILURE);
	assert_false(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

/* Test to validate bulk transfer polling can be initiated */
static void test_usb_input_bulk_transfer_polling(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	int ret = -1;
	int called = 0;
	unsigned char response[] = "Test response!";

	fake_init(deps->usb_dev, NULL);

	usb_set_bulk_transfer_context(deps->usb_dev, &called);

	/* create an arbitrary buffer of 100 bytes, this buffer will be attached to the
	 * usb session and freed during usb_deinit (during teardown), no need to manually free it */
	mock_libusb_get_max_packet_size(RETURN_SUCCESS);
	buffer_size = usb_bulk_transfer_response_buffer_init(deps->usb_dev, &buffer);

	/* mock libusb to fake an success transfer submit */
	mock_libusb_submit_transfer(LIBUSB_SUCCESS);

	ret = usb_input_bulk_transfer_polling(deps->usb_dev, buffer, buffer_size,
					      transfer_dispatcher_cb);
	assert_int_equal(ret, 0);

	/* simulate a response received from the I3c function */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response, sizeof(response));
	fake_transfer_set_transaction_status(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, LIBUSB_TRANSFER_COMPLETED);
	fake_transfer_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX);

	/* the transfer dispatcher callback should have been run */
	assert_int_equal(called, 1);
	assert_true(usb_input_bulk_transfer_polling_status(deps->usb_dev));
}

int main(void)
{
	/* Unit tests for the usb_input_bulk_transfer_polling() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_uninitialized_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_missing_buffer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_missing_callback, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_failure, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_transfer_failure, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_transfer_failure_after_callback, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usb_input_bulk_transfer_polling, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
