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

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to verify that the function handles a missing usb session gracefully */
static void test_negative_missing_usb_session(void **state)
{
	int ret = 0;

	ret = bulk_transfer_resume_request_async(NULL);
	assert_int_equal(ret, -1);
}

/* Test to verify that a request to resume a stalled command can be submitted */
static void test_resume_bulk_request(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);

	ret = bulk_transfer_resume_request_async(deps->usbi3c_dev->usb_dev);
	assert_int_equal(ret, 0);

	/* bulk_transfer_resume_request_async() is run asynchronously so we need to trigger
	 * a fake event about the transfer having completed */
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_resume_request_async() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_usb_session, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_resume_bulk_request, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
