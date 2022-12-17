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

int fake_handle = 1;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usbi3c_response *response = NULL;

	/* get a usbi3c context */
	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

	/* let's add some requests to the request tracker IDs 0,1 and 2 */
	response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	response->attempted = USBI3C_COMMAND_ATTEMPTED;
	response->error_status = USBI3C_SUCCEEDED;
	response->has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	response->data_length = 0;
	response->data = NULL;
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 0, 1, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 1, 1, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 2, 1, response);

	*state = deps;

	return 0;
}

static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, &fake_handle);
	free(deps);

	return 0;
}

static int create_mock_response_with_interrupt(unsigned char **buffer)
{
	uint32_t *dword_ptr = NULL;
	uint32_t buffer_size;

	/* for this response we only really care about the firs DW, but
	 * we'll still allocate 4 DW for it */
	buffer_size = 4 * DWORD_SIZE;
	*buffer = (unsigned char *)calloc(1, (size_t)buffer_size);
	assert_non_null(*buffer);

	dword_ptr = (uint32_t *)*buffer;
	/* bulk transfer IBI header (1 DW long) */
	*dword_ptr = 0x00000001; // Interrupt response = 0x1

	/* we only care about the header, so we don't need to
	 * add any more data to the buffer */

	return buffer_size;
}

/* test to verify the function handles receiving an invalid interrupt response gracefully.*/
static void test_negative_getting_an_invalid_interrupt_response(void **state)
{
	(void)state; /* unused */
	uint32_t response_buffer = 0x01;
	int response_buffer_size = sizeof(response_buffer);

	helper_trigger_response((uint8_t *)&response_buffer, response_buffer_size);
}

/* test to verify the function handles receiving an interrupt response gracefully.*/
static void test_negative_getting_an_interrupt_response(void **state)
{
	(void)state; /* unused */
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;

	response_buffer_size = create_mock_response_with_interrupt(&response_buffer);

	helper_trigger_response(response_buffer, response_buffer_size);

	free(response_buffer);
}

/* Negative test to verify the function handles receiving an unknown response gracefully.
 *
 * An unknown response is such that its request ID doesn't match any request  in the request
 * tracker, meaning the user did not send it. */
static void test_negative_getting_an_unknown_response(void **state)
{
	(void)state; /* unused */
	struct usbi3c_response expected_response;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = NULL;
	expected_response.data_length = 0;
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, 3); // ID=3 does not exist in the tracker

	/* simulate a response received from the I3C function, since the ID of the response
	 * is not recognized, it should simply be ignored */
	helper_trigger_response(response_buffer, response_buffer_size);

	free(response_buffer);
}

/* Negative test to verify the function handles receiving an already existing response gracefully.
 *
 * An existing response is such that a response is already in the request tracker */
static void test_negative_getting_an_existing_response(void **state)
{
	(void)state; /* unused */
	struct usbi3c_response expected_response;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = NULL;
	expected_response.data_length = 0;
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, 2); // ID=2 already has a response

	/* simulate a response received from the I3C function, since the ID of the response
	 * is not recognized, it should simply be ignored */
	helper_trigger_response(response_buffer, response_buffer_size);

	free(response_buffer);
}

/* This test focus is to validate that we can identify and parse a regular bulk response.
 *
 * This test validates the following:
 *  - Verifies the function uses the correct endpoint to receive data
 *  - Verifies the function is able to parse the response and appended data from the buffer
 *  - Verifies the function succeeds on receiving the response (using a mock transfer),
 *    and that returns the expected value
 */
static void test_getting_a_regular_response(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response expected_response;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;
	unsigned char data[] = "Some test data with a length of 35";
	int request_id;

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data_length = sizeof(data);
	expected_response.data = data;
	request_id = 0;
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, request_id);

	/* simulate a valid response received from the I3C function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* the response should have been added to the context tracker */
	struct regular_request *req = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id);
	assert_non_null(req);
	assert_non_null(req->response);

	/* verify that the response is accurate */
	assert_int_equal(req->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(req->response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_int_equal(req->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(req->response->data_length, sizeof(data));
	assert_memory_equal(req->response->data, data, sizeof(data));

	free(response_buffer);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_get_response() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_getting_an_invalid_interrupt_response),
		cmocka_unit_test(test_negative_getting_an_interrupt_response),
		cmocka_unit_test(test_negative_getting_an_unknown_response),
		cmocka_unit_test(test_negative_getting_an_existing_response),
		cmocka_unit_test(test_getting_a_regular_response),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
