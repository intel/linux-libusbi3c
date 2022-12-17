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

void on_vendor_response_cb(int response_data_size, void *response_data, void *user_data)
{
	/* intentionally left blank, we don't need to verify the callback was run in these tests */
}

/* Negative test to validate that the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int user_data = 0;
	int ret = 0;

	ret = usbi3c_on_vendor_specific_response(NULL, on_vendor_response_cb, &user_data);
	assert_int_equal(ret, RETURN_FAILURE);

	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb);
	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->user_data);
}

/* Negative test to validate that the function handles the user trying to set user data without a callback gracefully */
static void test_negative_user_data_without_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int user_data = 0;
	int ret = 0;

	ret = usbi3c_on_vendor_specific_response(deps->usbi3c_dev, NULL, &user_data);
	assert_int_equal(ret, RETURN_FAILURE);

	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb);
	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->user_data);
}

/* Test to validate that a callback can be assigned for the vendor response handler even with no user data */
static void test_usbi3c_on_vendor_specific_response_no_user_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_on_vendor_specific_response(deps->usbi3c_dev, on_vendor_response_cb, NULL);
	assert_int_equal(ret, RETURN_SUCCESS);

	assert_non_null(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb);
	assert_ptr_equal(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb, on_vendor_response_cb);
	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->user_data);
}

/* Test to validate that a callback and user data can be assigned for the vendor response handler */
static void test_usbi3c_on_vendor_specific_response(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int user_data = 0;
	int ret = -1;

	ret = usbi3c_on_vendor_specific_response(deps->usbi3c_dev, on_vendor_response_cb, &user_data);
	assert_int_equal(ret, RETURN_SUCCESS);

	assert_non_null(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb);
	assert_ptr_equal(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb, on_vendor_response_cb);
	assert_non_null(deps->usbi3c_dev->request_tracker->vendor_request->user_data);
	assert_ptr_equal(deps->usbi3c_dev->request_tracker->vendor_request->user_data, &user_data);
}

/* Test to validate that the callback and user data can be removed from the vendor response handler */
static void test_usbi3c_on_vendor_specific_response_remove_callback_and_user_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int user_data = 0;
	int ret = -1;

	deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb = on_vendor_response_cb;
	deps->usbi3c_dev->request_tracker->vendor_request->user_data = &user_data;

	ret = usbi3c_on_vendor_specific_response(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, RETURN_SUCCESS);

	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb);
	assert_null(deps->usbi3c_dev->request_tracker->vendor_request->user_data);
}

int main(void)
{
	/* Unit tests for the usbi3c_on_vendor_specific_response() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_user_data_without_callback, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_on_vendor_specific_response_no_user_data, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_on_vendor_specific_response, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_on_vendor_specific_response_remove_callback_and_user_data, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
