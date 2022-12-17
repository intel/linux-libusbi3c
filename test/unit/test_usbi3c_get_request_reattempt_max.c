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

/* Negative test to validate that the function handles a missing argument gracefully */
static void test_negative_missing_argument(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	unsigned int reattempt_max;
	int ret = 0;

	ret = usbi3c_get_request_reattempt_max(NULL, &reattempt_max);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_request_reattempt_max(&usbi3c_dev, NULL);
	assert_int_equal(ret, -1);
}

/* Test to validate that the max reattempts set can be retrieved */
static void test_usbi3c_get_request_reattempt_max(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct request_tracker request_tracker = { 0 };
	unsigned int reattempt_max;
	int ret = -1;

	request_tracker.reattempt_max = 4;
	usbi3c_dev.request_tracker = &request_tracker;

	ret = usbi3c_get_request_reattempt_max(&usbi3c_dev, &reattempt_max);
	assert_int_equal(ret, 0);
	assert_int_equal(reattempt_max, 4);
}

int main(void)
{
	/* Unit tests for the usbi3c_get_request_reattempt_max() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_usbi3c_get_request_reattempt_max),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
