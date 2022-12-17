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

void interrupt_dispatcher(void *interrupt_context, uint8_t *buffer, uint16_t len)
{
	unsigned char *result_buffer = (unsigned char *)interrupt_context;
	memcpy(result_buffer, buffer, len);
}

/* test USB interrupt initialization for an uninitialized device */
static void test_negative_uninitalized_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usb_interrupt_init(deps->usb_dev, interrupt_dispatcher);
	assert_int_equal(ret, -1);
}

/* test USB interrupt initialization handling an error from libusb transfer */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	fake_init(deps->usb_dev, NULL);

	/* mock the libusb function to fake an error */
	mock_libusb_submit_transfer(LIBUSB_ERROR_NO_DEVICE);

	ret = usb_interrupt_init(deps->usb_dev, interrupt_dispatcher);
	assert_int_equal(ret, LIBUSB_ERROR_NO_DEVICE);
}

/* test USB interrupt initialization handling a transfer failure */
static void test_negative_transfer_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int buffer_len = sizeof(struct notification_format);
	int ret = -1;
	unsigned char *buffer = (unsigned char *)malloc(buffer_len);
	unsigned char *result_buffer = (unsigned char *)malloc(buffer_len);
	memset(buffer, 10, buffer_len);

	fake_init(deps->usb_dev, NULL);

	/* mocks for libusb functions */
	mock_libusb_submit_transfer(RETURN_SUCCESS);

	usb_set_interrupt_buffer_length(deps->usb_dev, buffer_len);
	usb_set_interrupt_context(deps->usb_dev, result_buffer);
	ret = usb_interrupt_init(deps->usb_dev, interrupt_dispatcher);
	assert_int_equal(ret, 0);

	/* wait until we receive an async event
	 * (this will trigger the fake event set up in the mocks) */
	fake_transfer_add_data(USBI3C_INTERRUPT_ENDPOINT_INDEX, buffer, buffer_len);
	mock_libusb_wait_for_events_trigger(USBI3C_INTERRUPT_ENDPOINT_INDEX, RETURN_SUCCESS);
	fake_transfer_set_transaction_status(USBI3C_INTERRUPT_ENDPOINT_INDEX, LIBUSB_TRANSFER_STALL);
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(usb_get_errno(deps->usb_dev), LIBUSB_TRANSFER_STALL);

	free(buffer);
	free(result_buffer);
}

/* test USB interrupt initialization handling a libusb failure after calling its callback */
static void test_negative_transfer_failure_after_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int buffer_len = sizeof(struct notification_format);
	int ret = -1;
	unsigned char *buffer = (unsigned char *)malloc(buffer_len);
	unsigned char *result_buffer = (unsigned char *)malloc(buffer_len);
	memset(buffer, 10, buffer_len);

	fake_init(deps->usb_dev, NULL);

	/* mocks for libusb functions */
	mock_libusb_submit_transfer(1);
	fake_transfer_add_data(USBI3C_INTERRUPT_ENDPOINT_INDEX, buffer, buffer_len);
	mock_libusb_wait_for_events_trigger(USBI3C_INTERRUPT_ENDPOINT_INDEX, RETURN_SUCCESS);

	usb_set_interrupt_buffer_length(deps->usb_dev, buffer_len);
	usb_set_interrupt_context(deps->usb_dev, result_buffer);
	ret = usb_interrupt_init(deps->usb_dev, interrupt_dispatcher);
	assert_int_equal(ret, 0);

	/* wait until we receive an async event
	 * (this will trigger the fake event set up in the mocks) */
	usb_wait_for_next_event(deps->usb_dev);

	assert_int_equal(usb_get_errno(deps->usb_dev), RETURN_FAILURE);

	free(buffer);
	free(result_buffer);
}

/* Test that validates USB interrupts using the dispatcher and interrupt context */
static void test_usb_interrupt(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int buffer_len = sizeof(struct notification_format);
	int ret = -1;
	unsigned char *buffer = (unsigned char *)malloc(buffer_len);
	unsigned char *result_buffer = (unsigned char *)malloc(buffer_len);
	memset(buffer, 10, buffer_len);

	fake_init(deps->usb_dev, NULL);

	/* mocks for libusb functions */
	mock_libusb_submit_transfer(1);
	fake_transfer_add_data(USBI3C_INTERRUPT_ENDPOINT_INDEX, buffer, buffer_len);
	mock_libusb_wait_for_events_trigger(USBI3C_INTERRUPT_ENDPOINT_INDEX, RETURN_SUCCESS);

	usb_set_interrupt_buffer_length(deps->usb_dev, buffer_len);
	usb_set_interrupt_context(deps->usb_dev, result_buffer);
	ret = usb_interrupt_init(deps->usb_dev, interrupt_dispatcher);
	assert_int_equal(ret, 0);

	/* wait until we receive an async event
	 * (this will trigger the fake event set up in the mocks) */
	usb_wait_for_next_event(deps->usb_dev);

	assert_memory_equal(buffer, result_buffer, buffer_len);

	free(buffer);
	free(result_buffer);
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_uninitalized_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_failure, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_transfer_failure, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_transfer_failure_after_callback, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usb_interrupt, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
