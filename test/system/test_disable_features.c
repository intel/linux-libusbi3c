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

#include "usbi3c_i.h"

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_controller_init();
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* enable features manually */
	deps->usbi3c_dev->device_info->device_state.handoff_controller_role_enabled = TRUE;
	deps->usbi3c_dev->device_info->device_state.in_band_interrupt_enabled = TRUE;
	deps->usbi3c_dev->device_info->device_state.hot_join_enabled = TRUE;

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

/* Test to verify that the remote wake from a hot join request feature can be disabled */
static void test_usbi3c_disable_hot_join_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_hot_join_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that the hot-join feature can be disabled */
static void test_usbi3c_disable_hot_join(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that the I3C bus can be disabled */
static void test_usbi3c_disable_i3c_bus(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that the ability to handoff the I3C controller role be disabled */
static void test_usbi3c_disable_i3c_controller_role_handoff(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that the remote wake from a controller role request feature can be disabled */
static void test_usbi3c_disable_i3c_controller_role_request_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_i3c_controller_role_request_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that the remote wake from an IBI can be disabled */
static void test_usbi3c_disable_regular_ibi_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_regular_ibi_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that regular IBIs can be disabled in the bus */
static void test_usbi3c_disable_regular_ibi(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

/* Test to verify that we can force all I3C Target devices to exit HDR mode */
static void test_usbi3c_exit_hdr_mode_for_recovery(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	ret = usbi3c_exit_hdr_mode_for_recovery(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
}

int main(void)
{
	/* Unit tests for enabling features */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_hot_join_wake, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_hot_join, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_i3c_bus, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_i3c_controller_role_handoff, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_i3c_controller_role_request_wake, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_regular_ibi_wake, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_disable_regular_ibi, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_exit_hdr_mode_for_recovery, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
