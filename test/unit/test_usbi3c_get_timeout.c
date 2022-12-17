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

struct priv_usb_device {
	struct usb_device usb_dev;			      ///< public USB device
	struct libusb_device *libusb_device;		      ///< libusb device
	struct libusb_device_handle *handle;		      ///< libusb device handle
	struct usb_context *usb_ctx;			      ///< USB context to handle events
	unsigned int timeout;				      ///< timeout in milliseconds for usb transactions
	int libusb_errno;				      ///< error code libusb would return on failure
	struct libusb_transfer *interrupt_transfer;	      ///< USB interrupt transfer info
	interrupt_dispatcher_fn interrupt_dispatcher;	      ///< function to handle interrupts
	void *interrupt_context;			      ///< context to share with interrupt dispatcher
	unsigned char *interrupt_buffer;		      ///< interrupt data buffer
	unsigned int interrupt_buffer_length;		      ///< interrupt data buffer length
	struct libusb_transfer *bulk_transfer;		      ///< transfer entity for async bulk transfers
	bulk_transfer_dispatcher_fn bulk_transfer_dispatcher; ///< function to handle bulk response transfers
	unsigned char *bulk_transfer_buffer;		      ///< bulk transfer buffer
	void *bulk_transfer_context;			      ///< context to share with bulk transfer dispatcher
	uint8_t stop_events;				      ///< flag to stop event thread
};

/* Negative test to validate that the function handles a missing argument gracefully */
static void test_negative_missing_argument(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	unsigned int timeout;
	int ret = 0;

	ret = usbi3c_get_timeout(NULL, &timeout);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_timeout(&usbi3c_dev, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate that the function handles a missing usb device gracefully */
static void test_negative_missing_usb_device(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	unsigned int timeout;
	int ret = 0;

	ret = usbi3c_get_timeout(&usbi3c_dev, &timeout);
	assert_int_equal(ret, -1);
	assert_int_equal(timeout, 0);
}

/* Test to validate that the USB transaction timeout set can be retrieved */
static void test_usbi3c_get_timeout(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct priv_usb_device priv_usb_device = { 0 };
	unsigned int timeout;
	int ret = -1;

	priv_usb_device.timeout = 60;
	usbi3c_dev.usb_dev = &priv_usb_device.usb_dev;

	ret = usbi3c_get_timeout(&usbi3c_dev, &timeout);
	assert_int_equal(ret, 0);
	assert_int_equal(timeout, 60);
}

int main(void)
{
	/* Unit tests for the test_usbi3c_get_timeout() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_negative_missing_usb_device),
		cmocka_unit_test(test_usbi3c_get_timeout),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
