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

struct usb_context {
	struct libusb_context *libusb_context;
};

struct libusb_device {
	int fake_device_member;
};

/* Test to verify the function works properly when no devices are found */
static void test_usbi3c_get_devices_no_devices(void **state)
{
	struct usbi3c_context ctx = { 0 };
	struct usb_context usb_ctx = { 0 };
	struct usbi3c_device **devices = NULL;
	const int ANY = 0;
	int ret = -1;

	ctx.usb_ctx = &usb_ctx;
	mock_libusb_get_device_list(NULL, 0);

	ret = usbi3c_get_devices(&ctx, ANY, ANY, &devices);
	assert_int_equal(ret, 0);

	if (devices) {
		/* in case the test fails we need to free the resource */
		free(devices);
	}
}

int main(void)
{
	/* Unit tests for the usbi3c_get_devices() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_usbi3c_get_devices_no_devices),
		// TODO: add more tests for this function
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
