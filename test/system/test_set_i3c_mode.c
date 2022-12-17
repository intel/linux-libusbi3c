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

/* Test to verify that the mode and rate of the bus can be configured */
static void test_usbi3c_set_i3c_mode(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	const int RSTDAA = 0x06;
	const int TIMEOUT = 10;
	int ret = -1;

	usbi3c_set_i3c_mode(deps->usbi3c_dev, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_4_MHZ, 0);

	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, RSTDAA, 0, NULL, NULL, NULL);
	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* the command should*/
	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we sent only one command, we should have gotten only one response */
	assert_int_equal(list_len(responses), 1);

	/* verify that the response we got is correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);

	usbi3c_free_responses(&responses);
}

int main(void)
{
	/* Unit tests for the usbi3c_set_i3c_mode() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_usbi3c_set_i3c_mode, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
