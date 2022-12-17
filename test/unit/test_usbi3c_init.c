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
#include "mocks.h"

int fake_transfer = 1;

static int group_setup(void **state)
{
	/* enable the mocks required for these tests */
	enable_mock_libusb_alloc_transfer = TRUE;
	enable_mock_libusb_free_transfer = TRUE;

	return 0;
}

/* Negative test to verify that the function fails gracefully when usb_init fails */
static void test_negative_usb_fails_init(void **state)
{
	struct usbi3c_context *ctx = NULL;

	mock_libusb_init(NULL, RETURN_FAILURE);

	ctx = usbi3c_init();
	assert_null(ctx);

	if (ctx) {
		/* in case the test fails we need to free the resource */
		free(ctx);
	}
}

/* Verify that a usbi3c can be initialized correctly */
static void test_usbi3c_init(void **state)
{
	struct usbi3c_context *ctx = NULL;

	/* mock libusb_init */
	mock_libusb_init(NULL, RETURN_SUCCESS);
	/* mock the transfer deallocation for the interrupt and bulk transfers */

	ctx = usbi3c_init();
	assert_non_null(ctx);

	usbi3c_deinit(&ctx);
}

int main(void)
{

	/* Unit tests for the usbi3c_init() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_usb_fails_init),
		cmocka_unit_test(test_usbi3c_init),
	};

	return cmocka_run_group_tests(tests, group_setup, NULL);
}
