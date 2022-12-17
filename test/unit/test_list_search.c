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

int compare_integer(const void *a, const void *b)
{
	if ((*(int *)a == *(int *)b)) {
		return 0;
	} else if ((*(int *)a < *(int *)b)) {
		return -1;
	} else {
		return 1;
	}
}

/* Negative test to verify that when searching a list, if the item is not found, NULL is returned */
static void test_negative_list_search_not_found(void **state)
{
	(void)state; /* unused */
	struct list *list1 = NULL;
	int value_to_search = 6;
	int *value_found;

	list1 = helper_create_test_list(1, 5);

	value_found = (int *)list_search(list1, &value_to_search, compare_integer);
	assert_null(value_found);

	list_free_list_and_data(&list1, free);
}

/* Negative test to verify that when searching a list, if the node is not found, NULL is returned */
static void test_negative_list_search_node_not_found(void **state)
{
	(void)state; /* unused */
	struct list *list1 = NULL;
	int value_to_search = 6;
	int *value_found;

	list1 = helper_create_test_list(1, 5);

	value_found = (int *)list_search_node(list1, &value_to_search, compare_integer);
	assert_null(value_found);

	list_free_list_and_data(&list1, free);
}

/* Test to verify that a specific node can be searched for within a list, getting its data */
static void test_list_search(void **state)
{
	(void)state; /* unused */
	struct list *list1 = NULL;
	int value_to_search = 3;
	int *value_found;

	list1 = helper_create_test_list(1, 5);

	value_found = (int *)list_search(list1, &value_to_search, compare_integer);
	assert_int_equal(*value_found, 3);

	list_free_list_and_data(&list1, free);
}

/* Test to verify that a specific node can be searched for within a list, getting the node */
static void test_list_search_node(void **state)
{
	(void)state; /* unused */
	struct list *list1 = NULL;
	struct list *node = NULL;
	int value_to_search = 3;

	list1 = helper_create_test_list(1, 5);

	node = list_search_node(list1, &value_to_search, compare_integer);
	assert_int_equal(3, *(int *)node->data);

	list_free_list_and_data(&list1, free);
}

int main(void)
{

	/* Unit tests for the list_search() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_list_search_not_found),
		cmocka_unit_test(test_list_search),
		cmocka_unit_test(test_negative_list_search_node_not_found),
		cmocka_unit_test(test_list_search_node),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
