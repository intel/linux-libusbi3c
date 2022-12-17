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
	struct list *devices = NULL;

	devices = table_get_devices(NULL);
	assert_null(devices);
}

/* Test to validate that the function works properly when ther are no target devices in the table */
static void test_table_get_devices_empty_table(void **state)
{
	struct target_device_table table = { 0 };
	struct list *devices = NULL;

	devices = table_get_devices(&table);
	assert_null(devices);
}

/* Test to validate the function can get the list of target devices in the table */
static void test_table_get_devices(void **state)
{
	struct target_device_table table = { 0 };
	struct target_device device_1 = { 0 };
	struct target_device device_2 = { 0 };
	struct list *devices = NULL;

	table.target_devices = list_append(table.target_devices, &device_1);
	table.target_devices = list_append(table.target_devices, &device_2);

	devices = table_get_devices(&table);
	assert_non_null(devices);

	/* verify that only two devices are in the list */
	assert_non_null(devices->next);
	assert_null(devices->next->next);

	/* free the lists */
	list_free_list(&table.target_devices);
}

int main(void)
{
	/* Unit tests for the table_get_devices() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_table_get_devices_empty_table),
		cmocka_unit_test(test_table_get_devices),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
