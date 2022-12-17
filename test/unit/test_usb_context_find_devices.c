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

struct test_deps {
	struct usb_context *usb_ctx;
};

int setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	mock_libusb_init(NULL, RETURN_SUCCESS);
	usb_context_init(&deps->usb_ctx);
	*state = deps;
	return 0;
}

int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	usb_context_deinit(deps->usb_ctx);
	free(deps);
	return 0;
}

/* test usb find devices handling libusb error getting device list */
static void test_negative_usb_find_devices_fail(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_device **usb_devices = NULL;
	mock_libusb_get_device_list(NULL, LIBUSB_ERROR_IO);
	int ret = usb_find_devices(usb_ctx, NULL, &usb_devices);
	assert_int_equal(ret, LIBUSB_ERROR_IO);

	if (usb_devices) {
		/* in case the test fails we need to free the resource */
		free(usb_devices);
	}
}

/* test usb find devices finding no devices */
static void test_negative_usb_find_devices_no_devices(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_device **usb_devices = NULL;
	mock_libusb_get_device_list(NULL, 0);
	int ret = usb_find_devices(usb_ctx, NULL, &usb_devices);
	assert_int_equal(ret, 0);

	if (usb_devices) {
		/* in case the test fails we need to free the resource */
		free(usb_devices);
	}
}

struct libusb_device {
	int fake_device_member;
};

/* test usb find devices finding two devices and getting and libusb error getting device descriptor */
static void test_negative_usb_find_devices_no_descriptor(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_search_criteria criteria = { 0 };
	struct usb_device **usb_devices = NULL;

	struct libusb_device device_1 = { .fake_device_member = 1 };
	struct libusb_device device_2 = { .fake_device_member = 2 };
	struct libusb_device *libusb_devices[3];
	libusb_devices[0] = &device_1;
	libusb_devices[1] = &device_2;
	libusb_devices[2] = NULL;

	mock_libusb_get_device_list(libusb_devices, 2);
	mock_libusb_get_device_descriptor(0, 0, 0, LIBUSB_ERROR_NOT_FOUND);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int ret = usb_find_devices(usb_ctx, &criteria, &usb_devices);
	assert_null(usb_devices);

	/* an error should have been generated */
	assert_int_equal(ret, LIBUSB_ERROR_NOT_FOUND);

	if (usb_devices) {
		/* in case the test fails we need to free the resource */
		free(usb_devices);
	}
}

/* test usb find devices finding two devices but not matching the criteria */
static void test_negative_usb_find_devices_no_matching_devices(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_search_criteria criteria = { .dev_class = USBI3C_DeviceClass, .product_id = 2, .vendor_id = 100 };
	struct usb_device **usb_devices = NULL;

	struct libusb_device device_1 = { .fake_device_member = 1 };
	struct libusb_device device_2 = { .fake_device_member = 2 };
	struct libusb_device *libusb_devices[3];
	libusb_devices[0] = &device_1;
	libusb_devices[1] = &device_2;
	libusb_devices[2] = NULL;

	mock_libusb_get_device_list(libusb_devices, 2);
	mock_libusb_get_device_descriptor(0x10, 1, 100, RETURN_SUCCESS);
	mock_libusb_get_device_descriptor(0x10, 2, 100, RETURN_SUCCESS);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int ret = usb_find_devices(usb_ctx, &criteria, &usb_devices);
	assert_null(usb_devices);

	/* an error should have been generated */
	assert_int_equal(ret, 0);

	if (usb_devices) {
		/* in case the test fails we need to free the resource */
		free(usb_devices);
	}
}

/* test usb find devices finding two devices and one matching the criteria */
static void test_usb_find_devices_one_matching_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_search_criteria criteria = { .dev_class = USBI3C_DeviceClass, .product_id = 2, .vendor_id = 100 };
	struct usb_device **usb_devices = NULL;

	struct libusb_device device_1 = { .fake_device_member = 1 };
	struct libusb_device device_2 = { .fake_device_member = 2 };
	struct libusb_device *libusb_devices[3];
	libusb_devices[0] = &device_1;
	libusb_devices[1] = &device_2;
	libusb_devices[2] = NULL;

	mock_libusb_get_device_list(libusb_devices, 2);
	mock_libusb_get_device_descriptor(0x10, 1, 100, RETURN_SUCCESS);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&device_2);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int num_dev = usb_find_devices(usb_ctx, &criteria, &usb_devices);
	assert_int_equal(num_dev, 1);
	assert_non_null(usb_devices);
	assert_int_equal(usb_devices[0]->idProduct, 2);
	assert_int_equal(usb_devices[0]->idVendor, 100);
	usb_free_devices(usb_devices, num_dev);
}

/* test usb find devices finding two devices and both matching the criteria using the default criteria */
static void test_usb_find_devices_default_criteria(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_context *usb_ctx = deps->usb_ctx;
	struct usb_device **usb_devices = NULL;

	struct libusb_device device_1 = { .fake_device_member = 1 };
	struct libusb_device device_2 = { .fake_device_member = 2 };
	struct libusb_device *libusb_devices[3];
	libusb_devices[0] = &device_1;
	libusb_devices[1] = &device_2;
	libusb_devices[2] = NULL;

	mock_libusb_get_device_list(libusb_devices, 2);
	mock_libusb_get_device_descriptor(0x10, 1, 100, RETURN_SUCCESS);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&device_1);
	mock_libusb_ref_device(&device_2);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	int num_dev = usb_find_devices(usb_ctx, NULL, &usb_devices);
	assert_int_equal(num_dev, 2);
	assert_non_null(usb_devices);
	assert_int_equal(usb_devices[0]->idProduct, 1);
	assert_int_equal(usb_devices[0]->idVendor, 100);
	assert_int_equal(usb_devices[1]->idProduct, 2);
	assert_int_equal(usb_devices[1]->idVendor, 100);
	usb_free_devices(usb_devices, num_dev);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_usb_find_devices_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usb_find_devices_no_devices, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usb_find_devices_no_descriptor, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usb_find_devices_no_matching_devices, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usb_find_devices_one_matching_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usb_find_devices_default_criteria, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
