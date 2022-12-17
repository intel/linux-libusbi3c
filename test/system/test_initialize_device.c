/***************************************************************************
  USBI3C  -  Library to talk to I3C devices via USB.
  -------------------
  copyright            : (C) 2021 Intel Corporation
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

/* runs once before all tests */
static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **devices = NULL;
	int ret = -1;

	/* vendor and product ID of the USB I3C device we want to use */
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;

	/* initialize the library */
	ctx = usbi3c_init();

	/* set the I3C device for I/O operations */
	ret = usbi3c_get_devices(ctx, VENDOR_ID, PRODUCT_ID, &devices);
	assert_int_equal(ret, 1);
	deps->usbi3c_dev = usbi3c_ref_device(devices[0]);
	assert_non_null(deps->usbi3c_dev);

	usbi3c_deinit(&ctx);
	usbi3c_free_devices(&devices);

	*state = deps;

	return 0;
}

/* runs once after all tests */
static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	usbi3c_device_deinit(&deps->usbi3c_dev);
	free(deps);

	return 0;
}

/* Test to verify an I3C function that is connected to the USB bus can be initialized
 * as I3C main controller */
static void test_initialization_of_i3c_function_as_main_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* as part of I3C Function initialization, I3C Function shall configure
	 * the I3C Controller (Primary I3C Controller) */
	ret = usbi3c_initialize_device(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{

	/* Tests for the I3C function initialization */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_initialization_of_i3c_function_as_main_controller),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
