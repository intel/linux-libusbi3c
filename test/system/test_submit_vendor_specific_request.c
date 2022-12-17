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

#include "helpers/test_helpers.h"

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_controller_init();
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	usbi3c_device_deinit(&deps->usbi3c_dev);
	free(deps);

	return 0;
}

void vendor_response_cb(int data_size, void *data, void *user_data)
{
	int *callback_called = (int *)user_data;
	*callback_called = TRUE;
}

/* Test to verify generic vendor specific requests can be sent to the I3C function */
static void test_usbi3c_submit_vendor_specific_request(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char vendor_data[] = "data!";
	int callback_called = FALSE;
	int ret = -1;

	/* it is mandatory to specify a callback first */
	ret = usbi3c_on_vendor_specific_response(deps->usbi3c_dev, vendor_response_cb, &callback_called);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_vendor_specific_request(deps->usbi3c_dev, vendor_data, sizeof(vendor_data));
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	assert_true(callback_called);
}

int main(void)
{
	/* Unit tests for the usbi3c_submit_vendor_specific_request() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_submit_vendor_specific_request, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
