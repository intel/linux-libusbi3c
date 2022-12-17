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

/* Test to verify the device configuration can be changed */
static void test_usbi3c_set_target_device_config(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int DEVICE_ADDRESS = 0x05;
	const int IBIT = 0b100;
	const int CRR = 0b10;
	const int TIR = 0b1;
	int ret = -1;

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, DEVICE_ADDRESS, IBIT | CRR | TIR);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_set_target_device_config() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_set_target_device_config, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
