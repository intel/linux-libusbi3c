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

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_get_device_address(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an unkown device gracefully */
static void test_negative_unknown_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct device_info *tmp = NULL;
	int ret = 0;

	tmp = deps->usbi3c_dev->device_info;
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_get_device_address(deps->usbi3c_dev);
	assert_int_equal(ret, -1);

	deps->usbi3c_dev->device_info = tmp;
}

/* Test to verify the device address can be obtained */
static void test_usbi3c_get_device_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_get_device_address(deps->usbi3c_dev);
	assert_int_equal(ret, USBI3C_DEVICE_STATIC_ADDRESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_get_device_address() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_device, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_get_device_address, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
