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

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usb_device **usb_devices;
	mock_libusb_init(NULL, RETURN_SUCCESS);
	usb_context_init(&deps->usb_ctx);

	struct libusb_device libusb_device = { .fake_device_member = 1 };
	struct libusb_device *libusb_devices[2];
	libusb_devices[0] = &libusb_device;
	libusb_devices[1] = NULL;

	mock_libusb_get_device_list(libusb_devices, 1);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&libusb_device);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int num_dev = usb_find_devices(deps->usb_ctx, NULL, &usb_devices);
	assert_int_equal(num_dev, 1);
	assert_non_null(usb_devices);
	deps->usb_dev = usb_device_ref(usb_devices[0]);
	*state = deps;
	enable_mock_libusb_alloc_transfer = TRUE;
	enable_mock_libusb_free_transfer = TRUE;
	usb_free_devices(usb_devices, num_dev);
	return 0;
}

static int group_teardown(void **state)
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

/* Negative test to verify that function return -1 when USB device is not initialized */
static void test_negative_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usb_get_max_bulk_response_buffer_size(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to verify the function handles a libusb failure gracefully */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;
	int fake_transaction = 0;

	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(&fake_transaction);
	mock_libusb_alloc_transfer(&fake_transaction);
	usb_device_init(deps->usb_dev);

	/* mock libusb to fake an error */
	mock_libusb_get_max_packet_size(RETURN_FAILURE);

	ret = usb_get_max_bulk_response_buffer_size(deps->usb_dev);
	assert_int_equal(ret, LIBUSB_ERROR_NOT_FOUND);
}

/* Test to validate that the max bulk buffer size can be obtained */
static void test_usb_get_max_bulk_response_buffer_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	int fake_transaction = 0;

	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(&fake_transaction);
	mock_libusb_alloc_transfer(&fake_transaction);
	usb_device_init(deps->usb_dev);

	/* mock libusb to return a fake device */
	mock_libusb_get_max_packet_size(RETURN_SUCCESS);

	ret = usb_get_max_bulk_response_buffer_size(deps->usb_dev);
	assert_true(ret > 0);
}

int main(void)
{

	/* Unit tests for the usb_get_max_bulk_response_buffer_size() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_uninitialized_usb_device),
		cmocka_unit_test(test_negative_libusb_failure),
		cmocka_unit_test(test_usb_get_max_bulk_response_buffer_size),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
