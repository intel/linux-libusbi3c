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

const int DEVICES_IN_BUS = 1;
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

static int compare_device_address(const void *a, const void *b)
{
	if (((struct target_device *)a)->target_address < *(uint8_t *)b) {
		return -1;
	} else if (((struct target_device *)a)->target_address > *(uint8_t *)b) {
		return 1;
	}
	/* equal */
	return 0;
}

/* negative test to verify the function handles null parameters gracefully */
void test_negative_usbi3c_target_device_table_fill_from_device_table_buffer_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t buffer = 1;
	int ret = 0;

	ret = table_fill_from_device_table_buffer(NULL, &buffer, 1);
	assert_int_equal(ret, RETURN_FAILURE);
	ret = table_fill_from_device_table_buffer(deps->usbi3c_dev->target_device_table, NULL, 1);
	assert_int_equal(ret, RETURN_FAILURE);
	ret = table_fill_from_device_table_buffer(NULL, NULL, 1);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* negative test to verify the function handles an empty buffer gracefully */
void test_negative_usbi3c_target_device_table_fill_from_device_table_buffer_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t buffer = 0;
	int ret = 0;

	ret = table_fill_from_device_table_buffer(deps->usbi3c_dev->target_device_table, &buffer, 0);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* verifies that a target device table can be updated with a new device from the response of a GET_TARGET_DEVICE_TABLE request */
void test_table_fill_from_device_table_buffer_new_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	int ret = -1;
	int DEVICE_ADDRESS = INITIAL_TARGET_ADDRESS_POOL;

	/* create target device buffer buffer */
	buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	buffer_size = GET_TARGET_DEVICE_TABLE_HEADER(buffer)->table_size;

	ret = table_fill_from_device_table_buffer(deps->usbi3c_dev->target_device_table, buffer, buffer_size);
	assert_int_equal(ret, RETURN_SUCCESS);

	/* the device should have been added to the table */
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	assert_non_null(list_search(deps->usbi3c_dev->target_device_table->target_devices, &DEVICE_ADDRESS, compare_device_address));
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);

	free(buffer);
}

/* verifies that a target device table can be updated with an existing device from the response of a GET_TARGET_DEVICE_TABLE request */
void test_table_fill_from_device_table_buffer_update_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	int ret = -1;
	int DEVICE_ADDRESS = INITIAL_TARGET_ADDRESS_POOL;

	/* add an arbitrary device to address INITIAL_TARGET_ADDRESS_POOL */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_data.bus_characteristic_register = 0xAA;
	device->device_data.device_characteristic_register = 0xFF;
	device->target_address = DEVICE_ADDRESS;
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);

	/* create target device buffer buffer */
	buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	buffer_size = GET_TARGET_DEVICE_TABLE_HEADER(buffer)->table_size;

	ret = table_fill_from_device_table_buffer(deps->usbi3c_dev->target_device_table, buffer, buffer_size);
	assert_int_equal(ret, RETURN_SUCCESS);

	/* the device should have been updated in the table */
	pthread_mutex_lock(deps->usbi3c_dev->target_device_table->mutex);
	device = list_search(deps->usbi3c_dev->target_device_table->target_devices, &DEVICE_ADDRESS, compare_device_address);
	assert_non_null(device);
	assert_int_equal(device->device_data.bus_characteristic_register, INITIAL_BCR_POOL);
	assert_int_equal(device->device_data.device_characteristic_register, INITIAL_DCR_POOL);
	pthread_mutex_unlock(deps->usbi3c_dev->target_device_table->mutex);

	free(buffer);
}

int main(int argc, char *argv[])
{
	/* Unit tests for the table_fill_from_device_table_buffer() function */
	struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_fill_from_device_table_buffer_null, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_fill_from_device_table_buffer_empty, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_table_fill_from_device_table_buffer_new_device, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_table_fill_from_device_table_buffer_update_device, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
