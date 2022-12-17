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

/* Negative test to validate that the function handles a missing argument gracefully */
static void test_negative_missing_argument(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct usbi3c_target_device **target_devices = NULL;
	int ret = 0;

	ret = usbi3c_get_target_device_table(NULL, &target_devices);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_target_device_table(&usbi3c_dev, NULL);
	assert_int_equal(ret, -1);
}

/* Test to validate that the function works properly when ther are no target devices in the table */
static void test_usbi3c_get_devices_in_empty_table(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct target_device_table table = { 0 };
	struct usbi3c_target_device **target_devices = NULL;
	int ret = -1;

	usbi3c_dev.target_device_table = &table;

	ret = usbi3c_get_target_device_table(&usbi3c_dev, &target_devices);
	assert_int_equal(ret, 0);

	if (target_devices) {
		/* in case the test fails we need to free the resource */
		free(target_devices);
	}
}

/* Test to validate that the function can get the list of devices in the target device table */
static void test_usbi3c_get_devices_in_table(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct target_device_table table = { 0 };
	struct target_device device_1 = { 0 };
	struct target_device device_2 = { 0 };
	struct usbi3c_target_device **target_devices = NULL;
	struct usbi3c_target_device *device = NULL;
	int ret = -1;

	device_1.device_data.target_type = USBI3C_I3C_DEVICE;
	device_1.pid_hi = 0xAABBCCDD;
	device_1.pid_lo = 0xEEFF;
	device_1.target_address = 10;
	device_1.device_data.asa = USBI3C_DEVICE_HAS_NO_STATIC_ADDRESS;
	device_1.device_data.daa = 1;
	device_1.device_data.target_interrupt_request = 0; // accept
	device_1.device_data.controller_role_request = 1;  // reject
	device_1.device_data.ibi_timestamp = 1;
	device_1.device_data.max_ibi_payload_size = 1000000;
	table.target_devices = list_append(table.target_devices, &device_1);
	/* add one more device to the list */
	device_2.device_data.target_type = USBI3C_I2C_DEVICE;
	device_2.device_capability.static_address = 20;
	table.target_devices = list_append(table.target_devices, &device_2);
	usbi3c_dev.target_device_table = &table;

	ret = usbi3c_get_target_device_table(&usbi3c_dev, &target_devices);
	assert_int_equal(ret, 2);

	/* verify that only two devices are in the list */
	assert_non_null(target_devices[0]);
	assert_non_null(target_devices[1]);
	assert_null(target_devices[2]);

	/* verify that the data in the devices are accurate */
	device = (struct usbi3c_target_device *)target_devices[0];
	assert_int_equal(device->type, USBI3C_I3C_DEVICE);
	assert_int_equal(device->provisioned_id, 0x0000AABBCCDDEEFF);
	assert_int_equal(device->dynamic_address, 10);
	assert_int_equal(device->assignment_from_static_address, USBI3C_DEVICE_HAS_NO_STATIC_ADDRESS);
	assert_int_equal(device->dynamic_address_assignment_enabled, TRUE);
	assert_int_equal(device->target_interrupt_request_enabled, TRUE);
	assert_int_equal(device->controller_role_request_enabled, FALSE);
	assert_int_equal(device->ibi_timestamp_enabled, TRUE);
	assert_int_equal(device->max_ibi_payload_size, 1000000);

	device = (struct usbi3c_target_device *)target_devices[1];
	assert_int_equal(device->type, USBI3C_I2C_DEVICE);
	assert_int_equal(device->static_address, 20);

	usbi3c_free_target_device_table(&target_devices);
	list_free_list(&table.target_devices);
}

int main(void)
{
	/* Unit tests for the usbi3c_get_target_device_table() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_usbi3c_get_devices_in_empty_table),
		cmocka_unit_test(test_usbi3c_get_devices_in_table),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
