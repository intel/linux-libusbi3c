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

/* Test to validate the function can concatenate two lists into one */
static void test_list_concat(void **state)
{
	(void)state; /* unused */
	int values1[] = { 1, 2, 3, 4, 5 };
	int values2[] = { 4, 5, 1, 2, 3 };
	int i;

	struct list *list1 = NULL;
	struct list *list2 = NULL;
	struct list *final = NULL;

	list1 = helper_create_test_list(1, 3);
	list2 = helper_create_test_list(4, 5);

	final = list_concat(list1, list2);

	i = 0;
	for (struct list *node = final; node; node = node->next) {
		int *node_value = (int *)node->data;
		assert_int_equal(*node_value, values1[i]);
		i++;
	}

	list_free_list_and_data(&final, free);

	list1 = helper_create_test_list(1, 3);
	list2 = helper_create_test_list(4, 5);

	final = list_concat(list2, list1);

	i = 0;
	for (struct list *node = final; node; node = node->next) {
		int *node_value = (int *)node->data;
		assert_int_equal(*node_value, values2[i]);
		i++;
	}

	list_free_list_and_data(&final, free);
}

/* Test to validate that attempting to concatenate a list with NULL results in just returning
 * the original list */
static void test_list_concat_with_null(void **state)
{
	(void)state; /* unused */
	int values1[] = { 4, 5 };
	int values2[] = { 1, 2, 3 };
	int i;

	struct list *list1 = NULL;
	struct list *list2 = NULL;
	struct list *final = NULL;

	list2 = helper_create_test_list(4, 5);

	final = list_concat(NULL, list2);

	i = 0;
	for (struct list *node = final; node; node = node->next) {
		int *node_value = (int *)node->data;
		assert_int_equal(*node_value, values1[i]);
		i++;
	}

	list_free_list_and_data(&final, free);

	list1 = helper_create_test_list(1, 3);

	final = list_concat(list1, NULL);

	i = 0;
	for (struct list *node = final; node; node = node->next) {
		int *node_value = (int *)node->data;
		assert_int_equal(*node_value, values2[i]);
		i++;
	}

	list_free_list_and_data(&final, free);
}

int main(void)
{

	/* Unit tests for the list_concat() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_concat),
		cmocka_unit_test(test_list_concat_with_null),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
