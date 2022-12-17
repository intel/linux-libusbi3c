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

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static void create_test_table(struct target_device_table *table)
{
	struct target_device *device = NULL;

	/* I3C-only device */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->pid_hi = 0xAA01;
	device->pid_lo = 0xBB01;
	device->device_data.target_type = USBI3C_I3C_DEVICE;
	table->target_devices = list_append(table->target_devices, device);

	/* I2C-compatible I3C device */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->pid_hi = 0xAA02;
	device->pid_lo = 0xBB02;
	device->device_capability.static_address = 0x10;
	device->device_data.target_type = USBI3C_I3C_DEVICE;
	table->target_devices = list_append(table->target_devices, device);

	/* I2C device */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_capability.static_address = 0x0F;
	device->device_data.target_type = USBI3C_I2C_DEVICE;
	table->target_devices = list_append(table->target_devices, device);
}

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	create_test_table(deps->usbi3c_dev->target_device_table);

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

/* Negative test to validate that the function handles a missing table gracefully */
static void test_negative_missing_parameter(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int static_devices = 0;
	int dynamic_devices = 0;
	int ret = 0;

	ret = table_identify_devices(NULL, &static_devices, &dynamic_devices);
	assert_int_equal(ret, RETURN_FAILURE);

	ret = table_identify_devices(deps->usbi3c_dev->target_device_table, NULL, &dynamic_devices);
	assert_int_equal(ret, RETURN_FAILURE);

	ret = table_identify_devices(deps->usbi3c_dev->target_device_table, &static_devices, NULL);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to validate that the function handles an invalid I3C device (with no PID) gracefully */
static void test_negative_i3c_device_with_missing_pid(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	int static_devices = 0;
	int dynamic_devices = 0;
	int ret = 0;

	/* let's add an invalid device to the list */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);

	ret = table_identify_devices(deps->usbi3c_dev->target_device_table, &static_devices, &dynamic_devices);
	assert_int_equal(ret, RETURN_FAILURE);

	/* the counts should have been reset to 0 */
	assert_int_equal(static_devices, 0);
	assert_int_equal(dynamic_devices, 0);
}

/* Test to validate that the function identifies the address assignment mode required by the devices in the table */
static void test_table_identify_devices(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int static_devices = 0;
	int dynamic_devices = 0;
	int ret = -1;

	ret = table_identify_devices(deps->usbi3c_dev->target_device_table, &static_devices, &dynamic_devices);
	assert_int_equal(ret, RETURN_SUCCESS);
}

/* Test to validate that the function resets the devices count before starting the count */
static void test_table_reset_counts(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int static_devices = 10;
	int dynamic_devices = 10;
	int ret = -1;

	ret = table_identify_devices(deps->usbi3c_dev->target_device_table, &static_devices, &dynamic_devices);
	assert_int_equal(ret, RETURN_SUCCESS);

	/* regardless of the initial value our counters had before the function,
	 * they should have the correct values after the function has been run */
	assert_int_equal(static_devices, 2);
	assert_int_equal(dynamic_devices, 1);
}

int main(void)
{
	/* Unit tests for the table_identify_devices() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_parameter, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_i3c_device_with_missing_pid, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_table_identify_devices, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_table_reset_counts, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
