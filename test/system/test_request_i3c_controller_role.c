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

void on_controller_event_cb(enum usbi3c_controller_event_code event_code, void *user_data)
{
	*(int *)user_data = event_code;
}

/* Test to verify that users can request that a target device capable of controller role becomes the active controller */
static void test_usbi3c_request_i3c_controller_role(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	if (usbi3c_get_device_role(deps->usbi3c_dev) != USBI3C_TARGET_DEVICE_SECONDARY_CONTROLLER_ROLE || usbi3c_device_is_active_controller(deps->usbi3c_dev) == TRUE) {
		/* This test only applies when the usbi3c device is a target device capable of secondary
		 * controller role and it is currently not the active controller */
		skip();
	}

	ret = usbi3c_request_i3c_controller_role(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for the usbi3c_request_i3c_controller_role() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_request_i3c_controller_role, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
