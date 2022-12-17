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

/* Test to verify the function works properly when the list head is null */
static void test_list_len_null_head(void **state)
{
	int ret = -1;

	ret = list_len(NULL);
	assert_int_equal(ret, 0);
}

/* Test to verify the function can return the length of a linked list */
static void test_list_len(void **state)
{
	struct list *list = NULL;
	int ret = -1;

	list = helper_create_test_list(1, 5);
	ret = list_len(list);
	assert_int_equal(ret, 5);
	list_free_list_and_data(&list, free);

	list = helper_create_test_list(3, 5);
	ret = list_len(list);
	assert_int_equal(ret, 3);
	list_free_list_and_data(&list, free);

	list = helper_create_test_list(1, 1);
	ret = list_len(list);
	assert_int_equal(ret, 1);
	list_free_list_and_data(&list, free);
}

int main(void)
{
	/* Unit tests for the list_len() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_len_null_head),
		cmocka_unit_test(test_list_len),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
