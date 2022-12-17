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
	mock_libusb_init(NULL, RETURN_SUCCESS);
	struct usb_device **usb_devices;
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

/* Test input bulk transfer for an uninitialized device */
static void test_negative_input_bulk_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	int ret = -1;

	ret = usb_input_bulk_transfer(usb_dev, NULL, 0);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Test input bulk transfer handling libusb error */
static void test_negative_input_bulk_libusb_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint8_t data[1];
	int data_len = 1;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_IN;
	int ret = -1;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	expect_value(__wrap_libusb_bulk_transfer, endpoint, endpoint);
	will_return(__wrap_libusb_bulk_transfer, buffer);
	will_return(__wrap_libusb_bulk_transfer, buffer_size);
	will_return(__wrap_libusb_bulk_transfer, RETURN_FAILURE);

	ret = usb_input_bulk_transfer(usb_dev, data, data_len);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Test input bulk transfer success */
static void test_input_bulk_transfer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint8_t data[10];
	int data_len = 10;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_IN;
	int ret = -1;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	expect_value(__wrap_libusb_bulk_transfer, endpoint, endpoint);
	will_return(__wrap_libusb_bulk_transfer, buffer);
	will_return(__wrap_libusb_bulk_transfer, buffer_size);
	will_return(__wrap_libusb_bulk_transfer, RETURN_SUCCESS);

	ret = usb_input_bulk_transfer(usb_dev, data, data_len);
	assert_int_equal(ret, RETURN_SUCCESS);

	assert_memory_equal(data, buffer, data_len);
}

/* Test input bulk transfer with a buffer size different than the data length */
static void test_input_bulk_transfer_received_mismatched_expected(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint8_t data[5];
	int data_len = 5;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_IN;
	int ret = -1;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	expect_value(__wrap_libusb_bulk_transfer, endpoint, endpoint);
	will_return(__wrap_libusb_bulk_transfer, buffer);
	will_return(__wrap_libusb_bulk_transfer, buffer_size);
	will_return(__wrap_libusb_bulk_transfer, RETURN_SUCCESS);

	ret = usb_input_bulk_transfer(usb_dev, data, data_len);
	assert_int_equal(ret, RETURN_FAILURE);

	assert_memory_equal(data, buffer, data_len);
}

/* Test output bulk transfer for an uninitialized device */
static void test_negative_output_bulk_uninitialized_usb_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	int ret = -1;

	ret = usb_output_bulk_transfer(usb_dev, NULL, 0);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Test output bulk transfer handling libusb error */
static void test_negative_output_bulk_libusb_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_OUT;
	int ret = -1;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	mock_libusb_bulk_transfer(buffer, buffer_size, endpoint, RETURN_FAILURE);
	ret = usb_output_bulk_transfer(usb_dev, buffer, buffer_size);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Test output bulk transfer success */
static void test_output_bulk_transfer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_OUT;
	int ret = -1;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	mock_libusb_bulk_transfer(buffer, buffer_size, endpoint, RETURN_SUCCESS);
	ret = usb_output_bulk_transfer(usb_dev, buffer, buffer_size);
	assert_int_equal(ret, RETURN_SUCCESS);
}

/* Test output bulk transfer with a buffer size different than the data length */
static void test_output_bulk_transfer_received_mismatched_expected(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usb_device *usb_dev = deps->usb_dev;
	uint32_t endpoint = USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_OUT;
	int ret = -1;
	int data_transfered = 5;
	fake_init(usb_dev, NULL);

	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t buffer_size = 10;

	mock_libusb_bulk_transfer(buffer, data_transfered, endpoint, RETURN_SUCCESS);
	ret = usb_output_bulk_transfer(usb_dev, buffer, buffer_size);
	assert_int_equal(ret, RETURN_FAILURE);
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_input_bulk_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_input_bulk_libusb_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_input_bulk_transfer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_input_bulk_transfer_received_mismatched_expected, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_bulk_uninitialized_usb_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_output_bulk_libusb_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_output_bulk_transfer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_output_bulk_transfer_received_mismatched_expected, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
