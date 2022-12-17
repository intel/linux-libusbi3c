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
#include "usbi3c.h"

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

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_on_controller_event(NULL, NULL, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device with unknown capabilities gracefully */
static void test_negative_unkown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_on_controller_event(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device that is not a target device gracefully */
static void test_negative_device_is_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* this kind of notification is not supported by the active controller */
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

	ret = usbi3c_on_controller_event(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a device without an event handler gracefully */
static void test_negative_device_event_handler_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct device_event_handler *device_event_handler = NULL;
	int ret = 0;

	helper_initialize_target_device(deps->usbi3c_dev, NULL);

	/* let's remove the device event handler to force an error */
	device_event_handler = deps->usbi3c_dev->device_event_handler;
	deps->usbi3c_dev->device_event_handler = NULL;

	ret = usbi3c_on_controller_event(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, -1);

	/* cleanup */
	deps->usbi3c_dev->device_event_handler = device_event_handler;
}

static void on_controller_event_cb(enum usbi3c_controller_event_code event_code, void *data)
{
	/* intentionally left blank, we don't need to test the function was called in these tests */
	return;
}

/* Test to verify that a callback function can be assigned to the device event handler */
static void test_usbi3c_on_controller_event(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	helper_initialize_target_device(deps->usbi3c_dev, NULL);

	ret = usbi3c_on_controller_event(deps->usbi3c_dev, on_controller_event_cb, NULL);
	assert_int_equal(ret, 0);
}

/* Test to verify that a callback function and user data can be assigned to the device event handler */
static void test_usbi3c_on_controller_event_with_user_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int my_data = 1;
	int ret = 0;

	helper_initialize_target_device(deps->usbi3c_dev, NULL);

	ret = usbi3c_on_controller_event(deps->usbi3c_dev, on_controller_event_cb, &my_data);
	assert_int_equal(ret, 0);
}

int main(void)
{
	/* Unit tests for the usbi3c_on_controller_event() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_context, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_unkown_capabilities, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_device_is_active_controller, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_device_event_handler_null, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_on_controller_event, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_on_controller_event_with_user_data, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
