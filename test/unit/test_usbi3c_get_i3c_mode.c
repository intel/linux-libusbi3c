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
	uint8_t transfer_mode;
	uint8_t transfer_rate;
	uint8_t tm_specific_info;
	int ret = 0;

	ret = usbi3c_get_i3c_mode(NULL, &transfer_mode, &transfer_rate, &tm_specific_info);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_i3c_mode(&usbi3c_dev, NULL, &transfer_rate, &tm_specific_info);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_i3c_mode(&usbi3c_dev, &transfer_mode, NULL, &tm_specific_info);
	assert_int_equal(ret, -1);

	ret = usbi3c_get_i3c_mode(&usbi3c_dev, &transfer_mode, &transfer_rate, NULL);
	assert_int_equal(ret, -1);
}

/* Test to validate that the i3c mode set can be retrieved */
static void test_usbi3c_get_i3c_mode(void **state)
{
	struct usbi3c_device usbi3c_dev = { 0 };
	struct i3c_mode i3c_mode = { 0 };
	uint8_t transfer_mode;
	uint8_t transfer_rate;
	uint8_t tm_specific_info;
	int ret = -1;

	i3c_mode.transfer_mode = USBI3C_I3C_HDR_DDR_MODE;
	i3c_mode.transfer_rate = USBI3C_I3C_RATE_8_MHZ;
	i3c_mode.tm_specific_info = 0xAB;
	usbi3c_dev.i3c_mode = &i3c_mode;

	ret = usbi3c_get_i3c_mode(&usbi3c_dev, &transfer_mode, &transfer_rate, &tm_specific_info);
	assert_int_equal(ret, 0);

	assert_int_equal(transfer_mode, USBI3C_I3C_HDR_DDR_MODE);
	assert_int_equal(transfer_rate, USBI3C_I3C_RATE_8_MHZ);
	assert_int_equal(tm_specific_info, 0xAB);
}

int main(void)
{
	/* Unit tests for the usbi3c_get_i3c_mode() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_argument),
		cmocka_unit_test(test_usbi3c_get_i3c_mode),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
