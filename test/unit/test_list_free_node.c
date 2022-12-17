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

struct test_deps {
	struct list *list;
};

/* runs once before all tests */
static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct list *list_head = NULL;

	/* create a list with 4 nodes (values = 1, 2, 3, 4) */
	list_head = helper_create_test_list(1, 4);

	deps->list = list_head;
	*state = deps;

	return 0;
}

/* runs once after all tests */
static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *node = deps->list;

	while (node) {
		struct list *next = NULL;
		free(node->data);
		next = node->next;
		free(node);
		node = next;
	}
	deps->list = NULL;

	free(deps);

	return 0;
}

/* Test to verify a node from a list and its data can be freed */
static void test_list_free_node_and_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct list *node = NULL;

	/* remove a node and its data from the list */
	for (node = deps->list; node; node = node->next) {
		int *value = NULL;
		value = (int *)node->data;
		if (*value == 2) {
			deps->list = list_free_node(deps->list, node, free);
			break;
		}
	}

	node = deps->list;
	assert_int_equal(*(int *)node->data, 1);
	node = node->next;
	assert_int_equal(*(int *)node->data, 3);
	node = node->next;
	assert_int_equal(*(int *)node->data, 4);
	node = node->next;
	assert_null(node);
}

/* Test to verify a node can be freed from a list without freeing its data */
static void test_list_free_node_leave_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct list *node = NULL;
	int *ptr = NULL;

	/* remove a node from the list but leave its data */
	for (node = deps->list; node; node = node->next) {
		ptr = (int *)node->data;
		if (*ptr == 2) {
			deps->list = list_free_node(deps->list, node, NULL);
			break;
		}
	}

	node = deps->list;
	assert_int_equal(*(int *)node->data, 1);
	node = node->next;
	assert_int_equal(*(int *)node->data, 3);
	node = node->next;
	assert_int_equal(*(int *)node->data, 4);
	node = node->next;
	assert_null(node);

	/* the data should still exist even after the list has been removed */
	assert_non_null(ptr);
	assert_int_equal(*ptr, 2);

	free(ptr);
}

/* Test to verify the first node of the list can be freed */
static void test_list_free_first_node(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct list *node = NULL;

	/* remove the first node of the list */
	node = deps->list;
	deps->list = list_free_node(deps->list, node, free);

	node = deps->list;
	assert_non_null(node);
	assert_int_equal(*(int *)node->data, 2);
	node = node->next;
	assert_int_equal(*(int *)node->data, 3);
	node = node->next;
	assert_int_equal(*(int *)node->data, 4);
	node = node->next;
	assert_null(node);
}

/* Test to verify the last node of the list can be freed */
static void test_list_free_last_node(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct list *node = NULL;

	/* remove the last node of the list */
	for (node = deps->list; node; node = node->next) {
		int *value = NULL;
		value = (int *)node->data;
		if (*value == 4) {
			deps->list = list_free_node(deps->list, node, free);
			break;
		}
	}

	node = deps->list;
	assert_int_equal(*(int *)node->data, 1);
	node = node->next;
	assert_int_equal(*(int *)node->data, 2);
	node = node->next;
	assert_int_equal(*(int *)node->data, 3);
	node = node->next;
	assert_null(node);
}

/* Test to verify two consecutive nodes of the list can be freed */
static void test_list_free_two_consecutive_nodes(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct list *node = NULL;

	/* remove two consecutive nodes from the list */
	deps->list = list_free_node(deps->list, deps->list->next, free);
	deps->list = list_free_node(deps->list, deps->list->next, free);

	node = deps->list;
	assert_int_equal(*(int *)node->data, 1);
	node = node->next;
	assert_int_equal(*(int *)node->data, 4);
	node = node->next;
	assert_null(node);
}

int main(void)
{

	/* Unit tests for the list_free_node() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_list_free_node_and_data, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_list_free_node_leave_data, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_list_free_first_node, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_list_free_last_node, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_list_free_two_consecutive_nodes, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
