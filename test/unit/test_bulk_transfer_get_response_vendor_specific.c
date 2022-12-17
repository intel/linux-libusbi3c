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

static int create_mock_vendor_specific_response_buffer(unsigned char **expected_buffer)
{
	uint32_t *dword_ptr;
	uint32_t buffer_size;
	unsigned char test_data[] = "Arbitrary test data";

	/* let's allocate memory for the vendor response with data */
	buffer_size = (1 * DWORD_SIZE) + sizeof(test_data);
	*expected_buffer = (unsigned char *)calloc(1, (size_t)buffer_size);
	assert_non_null(expected_buffer);

	/* the first thing in the buffer should be the bulk transfer header (1 DW long) */
	dword_ptr = (uint32_t *)*expected_buffer;
	*dword_ptr = 0x00000002; // Vendor specific bulk response = 0x2

	/* let's say the vendor specific block only contains 20 bytes of data */
	memcpy((dword_ptr + 1), test_data, sizeof(test_data));

	return buffer_size;
}

/* runs once before all tests */
static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

	*state = deps;

	return 0;
}

/* runs once after all tests */
static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

void on_vendor_response_cb(int response_data_size, void *response_data, void *user_data)
{
	unsigned char **ptr = (unsigned char **)user_data;
	*ptr = (unsigned char *)calloc(1, response_data_size);
	memcpy(*ptr, response_data, response_data_size);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to receive data
 *  - Verifies the function is able to get the vendor data from the buffer
 *  - Verifies the function succeeds on receiving the response (using a mock transfer),
 *    and that returns the expected value
 */
static void test_getting_vendor_specific_response(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;
	unsigned char *user_data = NULL;

	deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb = on_vendor_response_cb;
	deps->usbi3c_dev->request_tracker->vendor_request->user_data = &user_data;

	/* let's allocate memory for the max_buffer size, which is what the
	 * function is going to do */
	response_buffer_size = create_mock_vendor_specific_response_buffer(&response_buffer);

	/* simulate a response received from the I3C function, the response type is
	 * for a vendor specific */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* the callback should have been called, and we should have been able to
	 * retrieve the data from the response into our current thread via user_data */
	assert_memory_equal(user_data, "Arbitrary test data", 20);

	free(response_buffer);
	free(user_data);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_get_response() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_getting_vendor_specific_response),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
