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

int fake_transfer = 1;

static int setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	mock_libusb_init(NULL, RETURN_SUCCESS);
	usb_context_init(&deps->usb_ctx);
	struct usb_device **usb_devices;

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

static int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	usb_context_deinit(deps->usb_ctx);
	free(deps);
	return 0;
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_negative_libusb_fails_open_handle(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_FAILURE);
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_device_is_initialized(deps->usb_dev));
	usb_device_deinit(deps->usb_dev);
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_negative_libusb_fails_detach_kernel_driver(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_FAILURE);
	mock_libusb_close();
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_device_is_initialized(deps->usb_dev));

	usb_device_deinit(deps->usb_dev);
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_negative_libusb_fails_claim_interface(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_FAILURE);
	mock_libusb_close();
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_device_is_initialized(deps->usb_dev));

	usb_device_deinit(deps->usb_dev);
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_negative_libusb_fails_interrupt_transfer_alloc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(NULL);
	mock_libusb_release_interface(NULL, 0);
	mock_libusb_close();
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_device_is_initialized(deps->usb_dev));

	usb_device_deinit(deps->usb_dev);
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_negative_libusb_fails_bulk_transfer_alloc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(&fake_transfer);
	mock_libusb_alloc_transfer(NULL);
	mock_libusb_release_interface(NULL, 0);
	mock_libusb_close();
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_false(usb_device_is_initialized(deps->usb_dev));

	usb_device_deinit(deps->usb_dev);
}

/* Negative test to validate that the function handles libusb errors gracefully */
static void test_init_deinit(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(&fake_transfer);
	mock_libusb_alloc_transfer(&fake_transfer);
	int ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_SUCCESS);
	assert_true(usb_device_is_initialized(deps->usb_dev));

	mock_libusb_release_interface(NULL, 0);
	mock_libusb_close();
	usb_device_deinit(deps->usb_dev);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_libusb_fails_open_handle, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_fails_detach_kernel_driver, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_fails_claim_interface, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_fails_interrupt_transfer_alloc, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_fails_bulk_transfer_alloc, setup, teardown),
		cmocka_unit_test_setup_teardown(test_init_deinit, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
