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

void on_hotjoin_cb(uint8_t address, void *user_data)
{
	*(int *)user_data = address;
}

/* Test to verify a callback function can be registered for hot-join events */
static void test_usbi3c_on_hotjoin(void **state)
{
	/* TODO: we currently have no way to generate a hot-join event in the I3C bus */
	skip();

	struct test_deps *deps = (struct test_deps *)*state;
	int hotjoin_address = 0;
	const int DEVICE_ADDRESS = 0x00; // TODO: update with the real device address

	usbi3c_on_hotjoin(deps->usbi3c_dev, on_hotjoin_cb, &hotjoin_address);
	// TODO: trigger the hot-join event from the target device
	assert_int_equal(hotjoin_address, DEVICE_ADDRESS);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_on_hotjoin() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_on_hotjoin, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
