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

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct device_info *device = NULL;

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	device = (struct device_info *)calloc(1, sizeof(struct device_info));
	deps->usbi3c_dev->device_info = device;
	deps->usbi3c_dev->device_info->device_role = USBI3C_PRIMARY_CONTROLLER_ROLE;
	deps->usbi3c_dev->device_info->data_type = STATIC_DATA;

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

/* Verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	(void)state; /* unused */
	int ret = 0;

	ret = initialize_i3c_bus(NULL);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Verify the function handles an unknown device gracefully */
static void test_negative_unknown_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct device_info *temp = NULL;
	int ret = 0;

	temp = deps->usbi3c_dev->device_info;
	deps->usbi3c_dev->device_info = NULL;

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);

	deps->usbi3c_dev->device_info = temp;
}

/* Verify the function handles a device with the wrong role gracefully */
static void test_negative_wrong_device_role(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* only primary controller can initialize the bus */
	deps->usbi3c_dev->device_info->device_role = USBI3C_TARGET_DEVICE_ROLE;

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Negative test to verify that the function handles an error in libusb gracefully */
static void test_negative_libusb_error(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_FAILURE);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* Test to verify that a request to initialize the I3C bus can be sent successfully */
static void test_initialize_i3c_bus_aware(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Test to verify that the bus can be initialized when the controller is not aware of target devices,
 * and only devices with static addresses are in the table */
static void test_initialize_i3c_bus_not_aware_i2c_devices_only(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	int ret = -1;

	deps->usbi3c_dev->device_info->data_type = NO_STATIC_DATA;

	/* add some devices to the table */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_capability.static_address = 0x10;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_capability.static_address = 0x20;
	device->pid_hi = 0x0F0F;
	device->pid_lo = 0xF0F0;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, SET_STATIC_ADDRESS_AS_DYNAMIC_ADDRESS, RETURN_SUCCESS);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Test to verify that the bus can be initialized when the controller is not aware of target devices,
 * and only devices which require dynamic addresses are in the table */
static void test_initialize_i3c_bus_not_aware_i3c_devices_only(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	int ret = -1;

	deps->usbi3c_dev->device_info->data_type = NO_STATIC_DATA;

	/* add some devices to the table */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->pid_hi = 0x0A0A;
	device->pid_lo = 0xA0A0;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->pid_hi = 0x0F0F;
	device->pid_lo = 0xF0F0;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, ENTER_DYNAMIC_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Test to verify that the bus can be initialized when the controller is not aware of target devices,
 * and there is a mix of devices which require dynamic addresses, and devices with static addresses are in the table */
static void test_initialize_i3c_bus_not_aware_mixed_devices(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = NULL;
	int ret = -1;

	deps->usbi3c_dev->device_info->data_type = NO_STATIC_DATA;

	/* add some devices to the table */
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->pid_hi = 0x0A0A;
	device->pid_lo = 0xA0A0;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);
	device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_capability.static_address = 0x10;
	device->pid_hi = 0x0F0F;
	device->pid_lo = 0xF0F0;
	deps->usbi3c_dev->target_device_table->target_devices = list_append(deps->usbi3c_dev->target_device_table->target_devices, device);

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Test to verify that a request to initialize the I3C bus falls back to using ENTDAA if the controller
 * is not aware of target devices and the user does not provide a target device table */
static void test_initialize_i3c_bus_not_aware_no_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	deps->usbi3c_dev->device_info->data_type = NO_STATIC_DATA;

	/* the request to initialize the bus is submitted successfully */
	mock_initialize_i3c_bus(NULL, ENTER_DYNAMIC_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	ret = initialize_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

int main(void)
{
	/* Unit tests for the initialize_i3c_bus() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unknown_device, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_wrong_device_role, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_libusb_error, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_initialize_i3c_bus_aware, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_initialize_i3c_bus_not_aware_i2c_devices_only, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_initialize_i3c_bus_not_aware_i3c_devices_only, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_initialize_i3c_bus_not_aware_mixed_devices, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_initialize_i3c_bus_not_aware_no_table, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
