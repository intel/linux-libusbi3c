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

/* Negative test to verify the function handles a NULL list gracefully */
static void test_negative_null_list(void **state)
{
	(void)state; /* unused */
	struct list *list = NULL;

	/* if the list is already NULL, it should not break */
	list_free_list_and_data(&list, free);
	assert_null(list);
}

/* Test to verify that the function deletes the list and its data, and assigns NULL to the list pointer */
static void test_free_list_and_data(void **state)
{
	(void)state; /* unused */
	struct list *list = NULL;

	list = helper_create_test_list(1, 2);

	/* the list shoule be deleted along with its data */
	list_free_list_and_data(&list, free);
	assert_null(list);
}

/* Test to verify that the function deletes a list but leaves its data, and assigns NULL to the list pointer */
static void test_free_list(void **state)
{
	(void)state; /* unused */
	struct list *list = NULL;
	int *ptr1 = NULL;
	int *ptr2 = NULL;

	list = helper_create_test_list(1, 2);

	/* let's save a reference to the node's data  so we don't loose them */
	ptr1 = (int *)list->data;
	ptr2 = (int *)list->next->data;

	/* the list shoule be deleted but not its data */
	list_free_list(&list);
	assert_null(list);
	assert_int_equal(*ptr1, 1);
	assert_int_equal(*ptr2, 2);

	/* free the data */
	free(ptr1);
	free(ptr2);
}

int main(void)
{

	/* Unit tests for the list_free_list() and list_free_list_and_data() functions */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_null_list),
		cmocka_unit_test(test_free_list_and_data),
		cmocka_unit_test(test_free_list),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
