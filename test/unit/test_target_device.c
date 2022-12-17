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

/* test device_create_from_capability_entry don't break or allocate something when capability entry is NULL */
void test_negative_target_device_creation_from_capability_entry_null(void **state)
{
	struct target_device *device = device_create_from_capability_entry(NULL);
	assert_null(device);
}

/* test device_create_from_capability_entry */
void test_target_device_creation_from_capability_entry(void **state)
{
	struct capability_device_entry cap_entry = { 0 };

	cap_entry.pid_lo = 0x1;
	cap_entry.pid_hi = 0x2;
	cap_entry.ibi_prioritization = 0x3;
	cap_entry.mipi_disco_minor_version = 0x4;
	cap_entry.mipi_disco_major_version = 0x5;
	cap_entry.max_ibi_pending_size = 0x6;

	struct target_device *device = device_create_from_capability_entry(&cap_entry);

	assert_non_null(device);
	assert_int_equal(device->pid_lo, cap_entry.pid_lo);
	assert_int_equal(device->pid_hi, cap_entry.pid_hi);
	assert_int_equal(device->device_capability.ibi_prioritization, cap_entry.ibi_prioritization);
	assert_int_equal(device->device_capability.disco_minor_ver, cap_entry.mipi_disco_minor_version);
	assert_int_equal(device->device_capability.disco_major_ver, cap_entry.mipi_disco_major_version);
	assert_int_equal(device->device_capability.max_ibi_pending_read_size, cap_entry.max_ibi_pending_size);

	free(device);
}

/* test device_update_from_capability_entry don't fail when a parameter is NULL */
void test_negative_target_device_update_from_capability_entry_null(void **state)
{
	struct capability_device_entry cap_entry;
	struct target_device device;

	device_update_from_capability_entry(NULL, NULL);
	device_update_from_capability_entry(&device, NULL);
	device_update_from_capability_entry(NULL, &cap_entry);
}

/* test device_update_from_capability_entry */
void test_target_device_update_from_capability_entry(void **state)
{
	struct capability_device_entry cap_entry;
	struct target_device device = { 0 };

	cap_entry.pid_lo = 0x1;
	cap_entry.pid_hi = 0x2;
	cap_entry.ibi_prioritization = 0x3;
	cap_entry.mipi_disco_minor_version = 0x4;
	cap_entry.mipi_disco_major_version = 0x5;
	cap_entry.max_ibi_pending_size = 0x6;

	device_update_from_capability_entry(&device, &cap_entry);

	assert_int_not_equal(device.pid_lo, 0x1);
	assert_int_not_equal(device.pid_hi, 0x2);
	assert_int_equal(device.device_capability.ibi_prioritization, 0x3);
	assert_int_equal(device.device_capability.disco_minor_ver, 0x4);
	assert_int_equal(device.device_capability.disco_major_ver, 0x5);
	assert_int_equal(device.device_capability.max_ibi_pending_read_size, 0x6);
}

/* test device_create_from_device_table_entry don't fail when a parameter is NULL */
void test_negative_target_device_create_from_target_device_table_entry_null(void **state)
{
	struct target_device *device = device_create_from_device_table_entry(NULL);
	assert_null(device);
}

/* test device_create_from_device_table_entry */
void test_target_device_create_from_target_device_table_entry(void **state)
{
	struct target_device_table_entry entry = { 0 };

	entry.address = 0x1;
	entry.pid_lo = 0x2;
	entry.pid_hi = 0x3;
	entry.target_type = USBI3C_I2C_DEVICE;
	entry.dcr = 0x4;
	entry.bcr = 0x5;
	entry.target_interrupt_request = 0;
	entry.controller_role_request = 1;
	entry.ibi_timestamp = 0;
	entry.max_ibi_payload_size = 0x7;

	struct target_device *device = device_create_from_device_table_entry(&entry);

	assert_non_null(device);
	assert_int_equal(device->pid_lo, 0x2);
	assert_int_equal(device->pid_hi, 0x3);
	assert_int_equal(device->device_data.target_type, USBI3C_I2C_DEVICE);
	assert_int_equal(device->device_data.device_characteristic_register, 0x4);
	assert_int_equal(device->device_data.bus_characteristic_register, 0x5);
	assert_int_equal(device->device_data.target_interrupt_request, 0);
	assert_int_equal(device->device_data.controller_role_request, 1);
	assert_int_equal(device->device_data.ibi_timestamp, 0);
	assert_int_equal(device->device_data.max_ibi_payload_size, 0x7);
	free(device);
}

/* test device_update_from_device_table_entry don't fail when a parameter is NULL */
void test_negative_target_device_update_from_target_device_table_entry_null(void **state)
{
	struct target_device device;
	struct target_device_table_entry entry;

	device_update_from_device_table_entry(NULL, &entry);
	device_update_from_device_table_entry(&device, NULL);
	device_update_from_device_table_entry(NULL, NULL);
}

/* test device_update_from_device_table_entry */
void test_target_device_update_from_target_device_table_entry(void **state)
{
	struct target_device_table_entry entry = { 0 };
	struct target_device device = { 0 };

	entry.address = 0x1;
	entry.pid_lo = 0x2;
	entry.pid_hi = 0x3;
	entry.target_type = USBI3C_I3C_DEVICE;
	entry.dcr = 0x4;
	entry.bcr = 0x5;
	entry.target_interrupt_request = 0;
	entry.controller_role_request = 1;
	entry.ibi_timestamp = 0;
	entry.max_ibi_payload_size = 0x7;

	device_update_from_device_table_entry(&device, &entry);

	assert_int_not_equal(device.pid_lo, 0x2);
	assert_int_not_equal(device.pid_hi, 0x3);
	assert_int_equal(device.device_data.target_type, USBI3C_I3C_DEVICE);
	assert_int_equal(device.device_data.device_characteristic_register, 0x4);
	assert_int_equal(device.device_data.bus_characteristic_register, 0x5);
	assert_int_equal(device.device_data.target_interrupt_request, 0);
	assert_int_equal(device.device_data.controller_role_request, 1);
	assert_int_equal(device.device_data.ibi_timestamp, 0);
	assert_int_equal(device.device_data.max_ibi_payload_size, 0x7);
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_target_device_creation_from_capability_entry_null),
		cmocka_unit_test(test_target_device_creation_from_capability_entry),
		cmocka_unit_test(test_negative_target_device_update_from_capability_entry_null),
		cmocka_unit_test(test_target_device_update_from_capability_entry),
		cmocka_unit_test(test_negative_target_device_create_from_target_device_table_entry_null),
		cmocka_unit_test(test_target_device_create_from_target_device_table_entry),
		cmocka_unit_test(test_negative_target_device_update_from_target_device_table_entry_null),
		cmocka_unit_test(test_target_device_update_from_target_device_table_entry),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
