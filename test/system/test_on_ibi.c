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

#include "helpers/test_helpers.h"

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_controller_init();
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	usbi3c_device_deinit(&deps->usbi3c_dev);
	free(deps);

	return 0;
}

void on_ibi_cb(uint8_t report, struct usbi3c_ibi *descriptor, uint8_t *data, size_t size, void *user_data)
{
	*(int *)user_data = report;
}

/* Test to verify that users can assign a callback function to be triggered when an IBI is received */
static void test_usbi3c_on_ibi(void **state)
{
	/* TODO: we currently have no way to generate an IBI event in the I3C bus */
	skip();

	struct test_deps *deps = (struct test_deps *)*state;
	int report = 0;
	const int REPORT_NUMBER = 0; // TODO: update with the real report number to be triggered

	usbi3c_on_ibi(deps->usbi3c_dev, on_ibi_cb, &report);
	// TODO: trigger the IBI event
	assert_int_equal(report, REPORT_NUMBER);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_on_ibi() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_on_ibi, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
