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
#include "target_device_table_i.h"

const uint8_t ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
const uint8_t ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
const uint32_t NEW_MAX_IBI_PAYLOAD_SIZE = 0x800;

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

	ret = usbi3c_set_target_device_max_ibi_payload(NULL, ADDRESS_2, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device with unknown capabilities gracefully */
static void test_negative_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct device_info *tmp = NULL;
	int ret = 0;

	/* hide the device capabilities */
	tmp = deps->usbi3c_dev->device_info;
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_set_target_device_max_ibi_payload(deps->usbi3c_dev, ADDRESS_1, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, -1);

	deps->usbi3c_dev->device_info = tmp;
}

/* Negative test to verify the function handles a device that is not the active controller gracefully */
static void test_negative_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_set_target_device_max_ibi_payload(deps->usbi3c_dev, ADDRESS_1, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles an address with an empty device gracefully */
static void test_negative_empty_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	uint8_t empty_address = 1;

	ret = usbi3c_set_target_device_max_ibi_payload(deps->usbi3c_dev, empty_address, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a failure in libusb gracefully */
static void test_negative_libusb_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	unsigned char *buffer = NULL;

	/* create a fake control request that resembles the one we expect */
	buffer = mock_set_target_max_ibi_payload(ADDRESS_1, NEW_MAX_IBI_PAYLOAD_SIZE, RETURN_FAILURE);

	ret = usbi3c_set_target_device_max_ibi_payload(deps->usbi3c_dev, ADDRESS_1, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, -1);

	free(buffer);
}

/* Test to verify that a target device can be configured with a new max ibi payload size */
static void test_usbi3c_set_target_device_max_ibi_payload(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	unsigned char *buffer = NULL;
	int ret = -1;

	/* create a fake control request that resembles the one we expect */
	buffer = mock_set_target_max_ibi_payload(ADDRESS_2, NEW_MAX_IBI_PAYLOAD_SIZE, RETURN_SUCCESS);

	ret = usbi3c_set_target_device_max_ibi_payload(deps->usbi3c_dev, ADDRESS_2, NEW_MAX_IBI_PAYLOAD_SIZE);
	assert_int_equal(ret, 0);

	/* verify that the config was updated in the device */
	device = table_get_device(deps->usbi3c_dev->target_device_table, ADDRESS_2);
	assert_int_equal(device->device_data.max_ibi_payload_size, NEW_MAX_IBI_PAYLOAD_SIZE);

	free(buffer);
}

int main(void)
{

	/* Unit tests for the usbi3c_set_target_device_max_ibi_payload() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_capabilities, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_not_active_controller, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_empty_address, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_failure, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_set_target_device_max_ibi_payload, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
