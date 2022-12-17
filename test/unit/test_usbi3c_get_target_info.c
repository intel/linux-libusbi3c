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

const uint8_t ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
const uint8_t EMPTY_ADDRESS = 0;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	const int NUMBER_OF_DEVICES = 2;

	deps->usbi3c_dev = helper_usbi3c_init(NULL);

	/* we need to get a couple of devices into the target device table */
	helper_create_dummy_devices_in_target_device_table(deps->usbi3c_dev, NUMBER_OF_DEVICES);

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
static void test_negative_missing_context_getting_bcr(void **state)
{
	int ret = 0;

	ret = usbi3c_get_target_BCR(NULL, ADDRESS_1);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an empty address in the device table gracefully */
static void test_negative_empty_address_getting_bcr(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_get_target_BCR(deps->usbi3c_dev, EMPTY_ADDRESS);
	assert_int_equal(ret, -1);
}

/* Test to verify the BCR of a target device can be read */
static void test_usbi3c_get_target_bcr(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int EXPECTED_BCR = INITIAL_BCR_POOL;
	int ret = -1;

	ret = usbi3c_get_target_BCR(deps->usbi3c_dev, ADDRESS_1);
	assert_int_equal(ret, EXPECTED_BCR);
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context_getting_dcr(void **state)
{
	int ret = 0;

	ret = usbi3c_get_target_DCR(NULL, ADDRESS_1);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an empty address in the device table gracefully */
static void test_negative_empty_address_getting_dcr(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_get_target_DCR(deps->usbi3c_dev, EMPTY_ADDRESS);
	assert_int_equal(ret, -1);
}

/* Test to verify the DCR of a target device can be read */
static void test_usbi3c_get_target_dcr(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int EXPECTED_DCR = INITIAL_DCR_POOL;
	int ret = -1;

	ret = usbi3c_get_target_DCR(deps->usbi3c_dev, ADDRESS_1);
	assert_int_equal(ret, EXPECTED_DCR);
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context_getting_type(void **state)
{
	int ret = 0;

	ret = usbi3c_get_target_type(NULL, ADDRESS_1);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an empty address in the device table gracefully */
static void test_negative_empty_address_getting_type(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_get_target_type(deps->usbi3c_dev, EMPTY_ADDRESS);
	assert_int_equal(ret, -1);
}

/* Test to verify the type of a target device can be read */
static void test_usbi3c_get_target_type(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int EXPECTED_TYPE = USBI3C_I3C_DEVICE;
	int ret = -1;

	ret = usbi3c_get_target_type(deps->usbi3c_dev, ADDRESS_1);
	assert_int_equal(ret, EXPECTED_TYPE);
}

int main(void)
{

	/* Unit tests for the following functions:
	 * - usbi3c_get_target_BCR()
	 * - usbi3c_get_target_DCR()
	 * - usbi3c_get_target_type()
	 */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_context_getting_bcr),
		cmocka_unit_test(test_negative_empty_address_getting_bcr),
		cmocka_unit_test(test_usbi3c_get_target_bcr),
		cmocka_unit_test(test_negative_missing_context_getting_dcr),
		cmocka_unit_test(test_negative_empty_address_getting_dcr),
		cmocka_unit_test(test_usbi3c_get_target_dcr),
		cmocka_unit_test(test_negative_missing_context_getting_type),
		cmocka_unit_test(test_negative_empty_address_getting_type),
		cmocka_unit_test(test_usbi3c_get_target_type),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
