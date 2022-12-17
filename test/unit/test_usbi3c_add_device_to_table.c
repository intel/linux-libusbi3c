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

int callback_called = FALSE;

void on_hotjoin_cb(uint8_t address, void *user_data)
{
	callback_called = TRUE;
}

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);
	/* register a callback for table insertion events to make sure the
	 * callback is not called when adding a device to the table manually */
	usbi3c_on_hotjoin(deps->usbi3c_dev, on_hotjoin_cb, NULL);

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

/* Negative test to validate that the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	struct usbi3c_target_device device = { 0 };
	int ret = 0;

	ret = usbi3c_add_device_to_table(NULL, device);
	assert_int_equal(ret, -1);
}

/* Negative test to validate that the function handles attempting to add a device with an address already taken gracefully */
static void test_negative_address_already_taken(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_target_device device = { 0 };
	int ret = 0;

	device.static_address = 100;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, -1);

	assert_false(callback_called);
}

/* Negative test to validate that the function handles attempting to add a device with a PID already taken gracefully */
static void test_negative_pid_already_taken(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device_i = NULL;
	struct usbi3c_target_device device = { 0 };
	int ret = 0;

	/* device to add */
	device.type = USBI3C_I3C_DEVICE;
	device.static_address = 0;
	device.provisioned_id = 0x112233445566;

	/* let's add a device with the same PID to the table */
	device_i = table_get_device(deps->usbi3c_dev->target_device_table, 100);
	device_i->target_address = 0;
	device_i->pid_hi = 0x11223344;
	device_i->pid_lo = 0x5566;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, -1);

	assert_false(callback_called);
}

/* Negative test to validate that the function handles attempting to add an I2C device with no address gracefully */
static void test_negative_i2c_device_without_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_target_device device = { 0 };
	int ret = 0;

	device.type = USBI3C_I2C_DEVICE;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, -1);

	assert_false(callback_called);
}

/* Negative test to validate that the function handles attempting to add an I3C device with no address and no PID gracefully */
static void test_negative_i3c_device_without_pid(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_target_device device = { 0 };
	int ret = 0;

	device.type = USBI3C_I3C_DEVICE;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, -1);

	assert_false(callback_called);
}

/* Test to verify I2C devices can be added to the table */
static void test_usbi3c_add_i2c_device_to_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_target_device device = { 0 };
	struct target_device *device_i = NULL;
	int ret = -1;

	device.type = USBI3C_I2C_DEVICE;
	device.static_address = 200;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, 0);

	assert_false(callback_called);
	device_i = table_get_device(deps->usbi3c_dev->target_device_table, device.static_address);
	assert_non_null(device_i);
	assert_int_equal(device_i->device_data.target_type, USBI3C_I2C_DEVICE);
}

/* Test to verify I3C devices with no static address can be added to the table */
static void test_usbi3c_add_i3c_device_to_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device_i = NULL;
	struct usbi3c_target_device device = { 0 };
	int ret = -1;

	device.type = USBI3C_I3C_DEVICE;
	device.static_address = 0;
	device.provisioned_id = 0x112233445566;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, 0);

	assert_false(callback_called);
	device_i = table_get_device(deps->usbi3c_dev->target_device_table, device.static_address);
	assert_non_null(device_i);
	assert_int_equal(device_i->device_data.target_type, USBI3C_I3C_DEVICE);
}

/* Test to verify I3C devices with static address can be added to the table */
static void test_usbi3c_add_i3c_device_with_static_address_to_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device_i = NULL;
	struct usbi3c_target_device device = { 0 };
	int ret = -1;

	device.type = USBI3C_I3C_DEVICE;
	device.static_address = 200;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, 0);

	assert_false(callback_called);
	device_i = table_get_device(deps->usbi3c_dev->target_device_table, device.static_address);
	assert_non_null(device_i);
	assert_int_equal(device_i->device_data.target_type, USBI3C_I3C_DEVICE);
}

/* Test to verify I3C devices with static address and provisioned id can be added to the table */
static void test_usbi3c_add_i3c_device_with_static_address_and_pid_to_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device_i = NULL;
	struct usbi3c_target_device device = { 0 };
	int ret = -1;

	device.type = USBI3C_I3C_DEVICE;
	device.static_address = 200;
	device.provisioned_id = 0x112233445566;

	ret = usbi3c_add_device_to_table(deps->usbi3c_dev, device);
	assert_int_equal(ret, 0);

	assert_false(callback_called);
	device_i = table_get_device(deps->usbi3c_dev->target_device_table, device.static_address);
	assert_non_null(device_i);
	assert_int_equal(device_i->device_data.target_type, USBI3C_I3C_DEVICE);
	assert_true(device_i->pid_lo > 0);
	assert_true(device_i->pid_hi > 0);
	assert_true(device_i->device_data.valid_pid);
}

int main(void)
{
	/* Unit tests for the usbi3c_add_device_to_table() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_address_already_taken, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_pid_already_taken, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_i2c_device_without_address, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_i3c_device_without_pid, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_add_i2c_device_to_table, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_add_i3c_device_to_table, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_add_i3c_device_with_static_address_to_table, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_add_i3c_device_with_static_address_and_pid_to_table, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
