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
	uint8_t config;
	int ret = 0;
	const int ADDRESS = 5;

	ret = usbi3c_get_target_device_config(NULL, ADDRESS, &config);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_target_device_config(&usbi3c_dev, ADDRESS, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate that the function handles an empty table gracefully */
static void test_negative_empty_table(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	uint8_t config;
	int ret = 0;
	const int ADDRESS = 5;

	ret = usbi3c_get_target_device_config(&usbi3c_dev, ADDRESS, &config);
	assert_int_equal(ret, -1);
}

/* Test to validate that the target device configuration set can be retrieved */
static void test_usbi3c_get_target_device_config(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct target_device_table target_device_table = { 0 };
	struct list *table = NULL;
	struct target_device device = { 0 };
	uint8_t config;
	int ret = -1;
	const int ADDRESS = 5;

	device.target_address = ADDRESS;
	device.device_data.ibi_timestamp = 1;
	device.device_data.controller_role_request = 0;
	device.device_data.target_interrupt_request = 1;
	table = list_append(table, &device);
	target_device_table.target_devices = table;
	usbi3c_dev.target_device_table = &target_device_table;
	target_device_table.mutex = malloc_or_die(sizeof(pthread_mutex_t));
	pthread_mutex_init(target_device_table.mutex, NULL);

	ret = usbi3c_get_target_device_config(&usbi3c_dev, ADDRESS, &config);
	assert_int_equal(ret, 0);
	assert_int_equal(config, 0b0101);

	free(target_device_table.mutex);
	list_free_list(&table);
}

int main(void)
{
	/* Unit tests for the usbi3c_get_target_device_config() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_negative_empty_table),
		cmocka_unit_test(test_usbi3c_get_target_device_config),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
