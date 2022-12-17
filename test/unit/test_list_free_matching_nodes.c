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

int match_integer(const void *a, const void *b)
{
	if ((*(int *)a == *(int *)b)) {
		return 0;
	} else {
		return 1;
	}
}

/* Test to verify the function works fine when the list is empty */
static void test_list_free_matching_nodes_empty_list(void **state)
{
	struct list *list = NULL;
	int value_to_match = 0;

	list = list_free_matching_nodes(list, &value_to_match, match_integer, free);
	assert_null(list);
}

/* Test to verify all nodes matching a condition can be removed from a list */
static void test_list_free_matching_nodes_and_data(void **state)
{
	struct list *list1, *list2, *node = NULL;
	int value_to_match = 3;
	int count = 0;

	/* let's create a list with integers that go from 1 to 5 but having two 3s:
	 * 1, 2, 3, 3, 4, 5 */
	list1 = helper_create_test_list(1, 3);
	list2 = helper_create_test_list(3, 5);
	list1 = list_concat(list1, list2);

	list1 = list_free_matching_nodes(list1, &value_to_match, match_integer, free);
	assert_non_null(list1);

	/* the nodes with 3 should have been removed -> 1, 2, 4, 5 */
	for (node = list1; node; node = node->next) {
		count++;
		int value = *(int *)node->data;
		if (value == value_to_match) {
			fail();
		}
	}
	assert_int_equal(count, 4);

	list_free_list_and_data(&list1, free);
}

/* Test to verify all nodes matching a condition can be removed from a list, but the data
 * they point to can be left alone */
static void test_list_free_matching_nodes_and_leave_data(void **state)
{
	struct list *list1, *list2, *node = NULL;
	int *val1, *val2 = NULL;
	int value_to_match = 3;
	int count = 0;

	/* let's create a list with integers that go from 1 to 5 but having two 3s:
	 * 1, 2, 3, 3, 4, 5 */
	list1 = helper_create_test_list(1, 3);
	list2 = helper_create_test_list(3, 5);
	list1 = list_concat(list1, list2);

	/* to avoid causing memory leaks let's keep a pointer to the data in the nodes
	 * we are going to free from the list */
	node = list_search_node(list1, &value_to_match, match_integer);
	val1 = node->data;
	val2 = node->next->data;

	list1 = list_free_matching_nodes(list1, &value_to_match, match_integer, NULL);
	assert_non_null(list1);

	/* the nodes with 3 should have been removed -> 1, 2, 4, 5 */
	for (node = list1; node; node = node->next) {
		count++;
		int value = *(int *)node->data;
		if (value == value_to_match) {
			fail();
		}
	}
	assert_int_equal(count, 4);

	/* the data from the nodes should still be available */
	assert_non_null(val1);
	assert_int_equal(*val1, 3);
	assert_non_null(val2);
	assert_int_equal(*val2, 3);

	list_free_list_and_data(&list1, free);
	free(val1);
	free(val2);
}

/* Test to verify the function works correctly when removing nodes from the head and tail of the list */
static void test_list_free_matching_nodes_from_list_edges(void **state)
{
	struct list *list1, *node = NULL;
	int value_to_match = 5;
	int *val_for_head = NULL;
	int count = 0;

	/* let's create a list with integers that start and end in the node we want to free:
	 * 5, 1, 2, 3, 4, 5 */
	list1 = helper_create_test_list(1, 5);
	val_for_head = (int *)calloc(1, sizeof(int));
	*val_for_head = 5;
	list1 = list_prepend(list1, val_for_head);

	list1 = list_free_matching_nodes(list1, &value_to_match, match_integer, free);
	assert_non_null(list1);

	/* the nodes with 5 should have been removed -> 1, 2, 3, 4 */
	for (node = list1; node; node = node->next) {
		count++;
		int value = *(int *)node->data;
		if (value == value_to_match) {
			fail();
		}
	}
	assert_int_equal(count, 4);

	list_free_list_and_data(&list1, free);
}

/* Test to verify the function works fine when all nodes from a list get removed */
static void test_list_free_matching_nodes_free_all_nodes(void **state)
{
	struct list *list = NULL;
	int value_to_match = 1;

	/* let's create a list with only one element that is going to be removed*/
	list = helper_create_test_list(1, 1);

	list = list_free_matching_nodes(list, &value_to_match, match_integer, free);
	assert_null(list);
}

int match_integer_first_occurrence(const void *a, const void *b)
{
	static int match = FALSE;

	if ((*(int *)a == *(int *)b)) {
		if (match == TRUE) {
			/* we already found a match, so we can stop here */
			return -1;
		} else {
			match = TRUE;
			return 0;
		}
	} else {
		/* no match, keep looking */
		return 1;
	}
}

/* Test to verify we can stop the search for matches with a condition */
static void test_list_free_matching_nodes_only_first_match(void **state)
{
	struct list *list1, *list2, *node = NULL;
	int value_to_match = 3;
	int count = 0;
	int count_value = 0;

	/* let's create a list with integers that go from 1 to 5 but having two 3s:
	 * 1, 2, 3, 3, 4, 5 */
	list1 = helper_create_test_list(1, 3);
	list2 = helper_create_test_list(3, 5);
	list1 = list_concat(list1, list2);

	/* the comparison_fn should provide the condition to stop the search */
	list1 = list_free_matching_nodes(list1, &value_to_match, match_integer_first_occurrence, free);
	assert_non_null(list1);

	/* the nodes with 3 should have been removed -> 1, 2, 4, 5 */
	for (node = list1; node; node = node->next) {
		count++;
		int value = *(int *)node->data;
		if (value == value_to_match) {
			count_value++;
			if (count_value > 1) {
				fail();
			}
		}
	}
	assert_int_equal(count, 5);

	list_free_list_and_data(&list1, free);
}

int main(void)
{

	/* Unit tests for the list_free_matching_nodes() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_free_matching_nodes_empty_list),
		cmocka_unit_test(test_list_free_matching_nodes_and_data),
		cmocka_unit_test(test_list_free_matching_nodes_and_leave_data),
		cmocka_unit_test(test_list_free_matching_nodes_from_list_edges),
		cmocka_unit_test(test_list_free_matching_nodes_free_all_nodes),
		cmocka_unit_test(test_list_free_matching_nodes_only_first_match),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
