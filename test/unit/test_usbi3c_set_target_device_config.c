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
#include "target_device_table_i.h"

const uint8_t ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
const uint8_t ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;

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
	uint8_t config = 0;

	ret = usbi3c_set_target_device_config(NULL, ADDRESS_1, config);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device with unknown capabilities gracefully */
static void test_negative_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct device_info *tmp = NULL;
	int ret = 0;
	uint8_t config = 0;

	/* hide the device capabilities */
	tmp = deps->usbi3c_dev->device_info;
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, ADDRESS_1, config);
	assert_int_equal(ret, -1);

	deps->usbi3c_dev->device_info = tmp;
}

/* Negative test to verify the function handles a device that is not the active controller gracefully */
static void test_negative_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	uint8_t config = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, ADDRESS_1, config);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an address with an empty device gracefully */
static void test_negative_empty_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	uint8_t empty_address = 0;
	uint8_t config = 0;

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, empty_address, config);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a failure in libusb gracefully */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	uint8_t config = 0;
	unsigned char *buffer = NULL;

	/* create a fake control request that resembles the one we expect */
	buffer = mock_set_target_device_config(ADDRESS_1, 0, 0, 0, RETURN_FAILURE);

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, ADDRESS_1, config);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Test to verify that a target device can be configured */
static void test_usbi3c_set_target_device_config(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	int ret = -1;
	unsigned char *buffer = NULL;
	const int ACCEPT = 0;
	const int REJECT = 1;

	/*
	 * 7:3 Reserved, 2 IBIT, 1 CRR, 0 TIR
	 * IBIT = 0 -> Active I3C Controller shall not timestamp IBIs from this Target device
	 * CRR = 1 -> Active I3C Controller shall NOT ACCEPT the I3C controller role requests from Secondary I3C Controllers
	 * TIR = 0 -> Active I3C Controller shall ACCEPT interrupts from this Target device
	 */
	uint8_t config = 0b00000010;

	/* create a fake control request that resembles the one we expect */
	buffer = mock_set_target_device_config(ADDRESS_2, 0, 1, 0, RETURN_SUCCESS);

	ret = usbi3c_set_target_device_config(deps->usbi3c_dev, ADDRESS_2, config);
	assert_int_equal(ret, 0);

	/* verify that the config was updated in the device */
	device = table_get_device(deps->usbi3c_dev->target_device_table, ADDRESS_2);
	assert_int_equal(device->device_data.ibi_timestamp, ACCEPT);
	assert_int_equal(device->device_data.controller_role_request, REJECT);
	assert_int_equal(device->device_data.target_interrupt_request, ACCEPT);

	free(buffer);
}

int main(void)
{

	/* Unit tests for the usbi3c_set_target_device_config() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_capabilities, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_not_active_controller, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_empty_address, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_set_target_device_config, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
