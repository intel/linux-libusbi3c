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
	enable_mock_libusb_alloc_transfer = TRUE;
	enable_mock_libusb_free_transfer = TRUE;
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
	int fake_transfer = 1;
	int ret = -1;
	mock_libusb_open(fake_handle, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(fake_handle, 1);
	mock_libusb_detach_kernel_driver(fake_handle, RETURN_SUCCESS);
	mock_libusb_claim_interface(fake_handle, RETURN_SUCCESS);
	mock_libusb_alloc_transfer(&fake_transfer);
	mock_libusb_alloc_transfer(&fake_transfer);
	ret = usb_device_init(usb_dev);
	assert_int_equal(ret, RETURN_SUCCESS);
}

/* test input control transfer for an uninitialized device */
static void test_negative_input_control_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	ret = usb_input_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test input control transfer handling a libusb error */
static void test_negative_input_control_libusb_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct control_transfer_mock control_transfer;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	fake_init(deps->usb_dev, NULL);

	/* mock libusb to return an error */
	control_transfer.bmRequestType = 0b10100001;
	control_transfer.bRequest = REQUEST;
	control_transfer.wValue = VALUE;
	control_transfer.wIndex = INDEX;
	control_transfer.wLength = LENGTH;
	control_transfer.data = NULL;
	control_transfer.timeout = 1000;
	mock_libusb_control_transfer(NULL, &control_transfer, RETURN_FAILURE);

	ret = usb_input_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test input control transfer success */
static void test_input_control_transfer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct control_transfer_mock control_transfer;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	fake_init(deps->usb_dev, NULL);

	/* mock libusb to return an error */
	control_transfer.bmRequestType = 0b10100001;
	control_transfer.bRequest = REQUEST;
	control_transfer.wValue = VALUE;
	control_transfer.wIndex = INDEX;
	control_transfer.wLength = LENGTH;
	control_transfer.data = NULL;
	control_transfer.timeout = 1000;
	mock_libusb_control_transfer(NULL, &control_transfer, RETURN_SUCCESS);

	ret = usb_input_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_SUCCESS);
}

/* test output control transfer for an uninitialized device */
static void test_negative_output_control_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	ret = usb_output_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test output control transfer handling a libusb error */
static void test_negative_output_control_libusb_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct control_transfer_mock control_transfer;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	fake_init(deps->usb_dev, NULL);

	/* mock libusb to return an error */
	control_transfer.bmRequestType = 0b00100001;
	control_transfer.bRequest = REQUEST;
	control_transfer.wValue = VALUE;
	control_transfer.wIndex = INDEX;
	control_transfer.wLength = LENGTH;
	control_transfer.data = NULL;
	control_transfer.timeout = 1000;
	mock_libusb_control_transfer(NULL, &control_transfer, RETURN_FAILURE);

	ret = usb_output_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test output control transfer success */
static void test_output_control_transfer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct control_transfer_mock control_transfer;
	int ret = 0;
	const int REQUEST = 1;
	const int VALUE = 0;
	const int INDEX = 0;
	const int LENGTH = 0;

	fake_init(deps->usb_dev, NULL);

	usb_set_timeout(deps->usb_dev, 2000);

	/* mock libusb to return an error */
	control_transfer.bmRequestType = 0b00100001;
	control_transfer.bRequest = REQUEST;
	control_transfer.wValue = VALUE;
	control_transfer.wIndex = INDEX;
	control_transfer.wLength = LENGTH;
	control_transfer.data = NULL;
	control_transfer.timeout = 2000;
	mock_libusb_control_transfer(NULL, &control_transfer, RETURN_SUCCESS);

	ret = usb_output_control_transfer(deps->usb_dev, REQUEST, VALUE, INDEX, NULL, LENGTH);
	assert_int_equal(ret, RETURN_SUCCESS);
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_input_control_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_input_control_libusb_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_input_control_transfer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_control_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_control_libusb_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_output_control_transfer, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
