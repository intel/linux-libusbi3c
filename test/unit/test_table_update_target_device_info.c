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

const int NUMBER_OF_DEVICES = 3;
const int TARGET_CONFIG = 0b00000000;
const int TARGET_CAPABILITY = 0b1000000000;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	*state = deps;

	return 0;
}

int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to validate that the function handles an null table gracefully (no segfault) */
void test_negative_table_update_target_device_info_null_table(void **state)
{
	int ret = 0;

	ret = table_update_target_device_info(NULL);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to verify that the function handles an error in libusb gracefully */
void test_negative_table_update_target_device_info_transfer_fail(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int ret = 0;

	/* mock the response to a GET_TARGET_DEVICE_TABLE request with a failure */
	buffer = mock_get_target_device_table(NULL, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, LIBUSB_ERROR_NO_DEVICE);

	ret = table_update_target_device_info(deps->usbi3c_dev->target_device_table);
	assert_int_equal(ret, LIBUSB_ERROR_NO_DEVICE);

	/* there should be no devices in the table */
	assert_null(deps->usbi3c_dev->target_device_table->target_devices);

	free(buffer);
}

/* Verify that we can get the target device table from a GET_TARGET_DEVICE_TABLE request */
void test_table_update_target_device_info(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *node = NULL;
	struct target_device *device = NULL;
	uint8_t *buffer = NULL;
	int ret = -1;

	/* mock the response to a GET_TARGET_DEVICE_TABLE request */
	buffer = mock_get_target_device_table(NULL, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);

	ret = table_update_target_device_info(deps->usbi3c_dev->target_device_table);
	assert_int_equal(ret, RETURN_SUCCESS);

	/* there should be 3 devices in the table, with addresses 100, 101 and 102 */
	int count = 0;
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	node = deps->usbi3c_dev->target_device_table->target_devices;
	for (; node; node = node->next) {
		device = (struct target_device *)node->data;
		assert_int_equal(device->target_address, INITIAL_TARGET_ADDRESS_POOL + count);
		count++;
	}
	assert_int_equal(count, NUMBER_OF_DEVICES);
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);

	free(buffer);
}

int main(int argc, char *argv[])
{
	/* Unit tests for the table_update_target_device_info() function */
	struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_table_update_target_device_info_null_table, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_table_update_target_device_info_transfer_fail, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_table_update_target_device_info, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
