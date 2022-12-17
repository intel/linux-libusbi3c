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

#include "helpers.h"
#include "mocks.h"

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);

	*state = deps;

	return 0;
}

static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	uint8_t *address_list = NULL;
	int ret = 0;

	ret = usbi3c_get_address_list(NULL, &address_list);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an empty device table gracefully */
static void test_negative_empty_target_device_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *address_list = NULL;
	int ret = 0;

	ret = usbi3c_get_address_list(deps->usbi3c_dev, &address_list);

	/* verify that the return value is 0 since the table was empty,
	 * the address list should also be empty */
	assert_int_equal(ret, 0);
	assert_null(address_list);
	if (address_list) {
		/* in case the test fails we need to free the resource */
		free(address_list);
	}
}

/* Test to verify a list of device addresses can be obtained from a target device table */
static void test_usbi3c_get_address_list(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *address_list = NULL;
	int ret = -1;
	const uint8_t ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const uint8_t ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	const int NUMBER_OF_DEVICES = 2;

	/* we need to get a couple of devices into the target device table */
	helper_create_dummy_devices_in_target_device_table(deps->usbi3c_dev, NUMBER_OF_DEVICES);

	ret = usbi3c_get_address_list(deps->usbi3c_dev, &address_list);

	/* verify that 2 addresses were added to the list */
	assert_int_equal(ret, 2);
	assert_int_equal(address_list[0], ADDRESS_1);
	assert_int_equal(address_list[1], ADDRESS_2);

	free(address_list);
}

int main(void)
{

	/* Unit tests for the usbi3c_get_address_list() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_context),
		cmocka_unit_test(test_negative_empty_target_device_table),
		cmocka_unit_test(test_usbi3c_get_address_list),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
