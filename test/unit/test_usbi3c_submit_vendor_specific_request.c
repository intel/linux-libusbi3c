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

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	/* get a usbi3c context */
	deps->usbi3c_dev = helper_usbi3c_init(NULL);

	*state = deps;

	return 0;
}

static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	(void)state; /* unused */
	unsigned char *data = NULL;
	int ret;

	ret = usbi3c_submit_vendor_specific_request(NULL, data, 10);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a missing data gracefully */
static void test_negative_missing_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret;

	ret = usbi3c_submit_vendor_specific_request(deps->usbi3c_dev, NULL, 10);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a wrong data_size gracefully */
static void test_negative_missing_data_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char data[] = "Arbitrary test data";
	int ret;

	ret = usbi3c_submit_vendor_specific_request(deps->usbi3c_dev, data, 0);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a missing on_vendor_response_cb callback gracefully */
static void test_negative_missing_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char data[] = "Arbitrary test data";
	int ret;

	ret = usbi3c_submit_vendor_specific_request(deps->usbi3c_dev, data, sizeof(data));
	assert_int_equal(ret, -1);
}

static uint32_t create_mock_request_buffer(unsigned char *test_data, uint32_t data_size, unsigned char **padded_data)
{
	uint32_t bytes_transferred;
	uint32_t *dword_ptr;
	uint8_t *byte_ptr;
	int padding;

	/* test data that is not 32-bit aligned, the data's size is 34 bytes,
	 * to be 32-bit aligned it needs to be padded.
	 * So we would expect the actual bytes transferred to be 36.
	 * In addition to those 36 bytes of data we have 4 bytes of header,
	 * so a total of 40 bytes to be transmitted */
	bytes_transferred = 40;
	padding = 2; // 36 bytes - 34 bytes

	/* lets create a memory space equivalent to the buffer we expect to receive */
	*padded_data = (unsigned char *)calloc(bytes_transferred, sizeof(unsigned char));
	assert_non_null(*padded_data);

	/* Let's build our reference memory map that resembles the one we
	 * expect for this test transaction */
	dword_ptr = (uint32_t *)*padded_data;
	/* bulk request transfer header (1 DW long) */
	*dword_ptr = 0x00000002;
	/* data block (34 bytes + 2 padding byte = 9 DW long) */
	byte_ptr = (uint8_t *)(dword_ptr + 1);
	memset(byte_ptr, 0, padding); // padded data should be zeroed
	memcpy((byte_ptr + padding), test_data, data_size);

	return bytes_transferred;
}

void on_vendor_response_cb(int response_data_size, void *response_data, void *user_data)
{
	/* intentionally left blank, we don't need to verify the callback was run in these tests */
}

/* Test to verify a vendor specific bulk request can be submitted and that the user data is padded correctly */
static void test_data_buffer_correctness(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char test_data[] = "Arbitrary test data - unaligned -"; // 34 bytes
	unsigned char *padded_data = NULL;
	uint32_t data_size = 34;
	uint32_t expected_bytes_transferred;
	int ret = 1;

	deps->usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb = on_vendor_response_cb;
	deps->usbi3c_dev->request_tracker->vendor_request->user_data = NULL;

	expected_bytes_transferred = create_mock_request_buffer(test_data, data_size, &padded_data);

	/* mock function for outgoing bulk transfers */
	mock_usb_output_bulk_transfer(padded_data, expected_bytes_transferred, RETURN_SUCCESS);

	ret = usbi3c_submit_vendor_specific_request(deps->usbi3c_dev, test_data, data_size);
	assert_int_equal(ret, 0);

	free(padded_data);
}

int main(void)
{

	/* Unit tests for the usbi3c_submit_vendor_specific_request() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_context),
		cmocka_unit_test(test_negative_missing_data),
		cmocka_unit_test(test_negative_missing_data_size),
		cmocka_unit_test(test_negative_missing_callback),
		cmocka_unit_test(test_data_buffer_correctness)
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
