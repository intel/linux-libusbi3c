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

void on_bus_error_cb(uint8_t error, void *user_data)
{
	*(int *)user_data = error;
}

/* Test to verify that users can assign a callback function to be triggered when an error in the I3C bus occurrs */
static void test_usbi3c_on_bus_error(void **state)
{
	/* TODO: we currently have no way to generate a bus error event in the I3C bus */
	skip();

	struct test_deps *deps = (struct test_deps *)*state;
	int bus_error = 0;
	const int ERROR_NUMBER = 0; // TODO: update with the real error number to be triggered

	usbi3c_on_bus_error(deps->usbi3c_dev, on_bus_error_cb, &bus_error);
	// TODO: trigger the bus error event
	assert_int_equal(bus_error, ERROR_NUMBER);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_on_bus_error() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_on_bus_error, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
