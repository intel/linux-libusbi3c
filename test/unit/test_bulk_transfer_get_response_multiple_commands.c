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
	struct list *expected_responses;
};

/* creates a buffer with multiple responses in which one of the commands failed
 * and the rest were not attempted */
static int create_mock_response_with_failure(unsigned char **buffer, struct request_tracker *request_tracker)
{
	int buffer_size;
	uint32_t *dword_ptr = NULL;
	uint8_t *byte_ptr = NULL;

	buffer_size = (1 * DWORD_SIZE);	      // transfer header size
	buffer_size += (3 * DWORD_SIZE) + 52; // first response size
	buffer_size += (3 * DWORD_SIZE);      // second response size
	buffer_size += (1 * DWORD_SIZE);      // third response size
	buffer_size += (1 * DWORD_SIZE);      // fourth response size
	*buffer = (unsigned char *)calloc(1, buffer_size);
	assert_non_null(*buffer);

	/*
	 * There is only one Bulk  Transfer Header for all responses (1 DW long)
	 */
	dword_ptr = (uint32_t *)*buffer;
	*dword_ptr = 0x00000000;

	/*
	 * first response
	 */
	/* response block header (1 DW long) */
	*(dword_ptr += 1) = 0b00000011000000000000000000000000 + 3; // attempted + has data, ID = 3
	/* response descriptor (2 DW long) */
	*(dword_ptr += 1) = 0x00000033; // error status = 0, data length = 51 (0x33)
	*(dword_ptr += 1) = 0x00000000; // reserved
	/* data block (51 bytes + 1 padding byte = 13 DW long) */
	byte_ptr = (uint8_t *)(dword_ptr += 1);
	memcpy((byte_ptr + 1), "Some arbitrary test data with a length of 51 bytes", 51);

	/*
	 * second response
	 */
	/* response block header (1 DW long) */
	*(dword_ptr += 13) = 0b00000010000000000000000000000000 + 4; // attempted + no data, ID = 4
	/* response descriptor (2 DW long) */
	*(dword_ptr += 1) = 0b00110000000000000000000000000000; // error status = 3 (frame error), data length = 0
	*(dword_ptr += 1) = 0x00000000;				// reserved
	/* data block (0 bytes) */

	/* The previous command failed, so the rest of the responses won't be attempted */

	/*
	 * third response
	 */
	/* response block header (1 DW long) */
	*(dword_ptr += 1) = 0b00000000000000000000000000000000 + 5; // not attempted + no data, ID = 5
	/* response descriptor (0 bytes) */
	/* data block (0 bytes) */

	/*
	 * fourth response
	 */
	/* response block header (1 DW long) */
	*(dword_ptr += 1) = 0b00000000000000000000000000000000 + 6; // not attempted + no data, ID = 6
	/* response descriptor (0 bytes) */
	/* data block (0 bytes) */

	/* let's add some requests to the request tracker IDs starting at 3 */
	helper_add_request_to_tracker(request_tracker, 3, 4, NULL);
	helper_add_request_to_tracker(request_tracker, 4, 4, NULL);
	helper_add_request_to_tracker(request_tracker, 5, 4, NULL);
	helper_add_request_to_tracker(request_tracker, 6, 4, NULL);

	return buffer_size;
}

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usbi3c_response *r1, *r2, *r3 = NULL;
	unsigned char data1[] = "Some test data with a length of 35";
	unsigned char data2[] = "Some data";

	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);
	deps->expected_responses = NULL;
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

	/* let's create the mock responses that are going to be "received" async from the I3C device */
	r1 = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	r1->attempted = USBI3C_COMMAND_ATTEMPTED;
	r1->has_data = USBI3C_RESPONSE_HAS_DATA;
	r1->error_status = USBI3C_SUCCEEDED;
	r1->data_length = sizeof(data1);
	r1->data = (unsigned char *)calloc(r1->data_length, sizeof(unsigned char));
	memcpy(r1->data, data1, r1->data_length);
	deps->expected_responses = list_append(deps->expected_responses, r1);

	r2 = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	r2->attempted = USBI3C_COMMAND_ATTEMPTED;
	r2->has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r2->error_status = USBI3C_SUCCEEDED;
	r2->data_length = 0;
	r2->data = NULL;
	deps->expected_responses = list_append(deps->expected_responses, r2);

	r3 = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	r3->attempted = USBI3C_COMMAND_ATTEMPTED;
	r3->has_data = USBI3C_RESPONSE_HAS_DATA;
	r3->error_status = USBI3C_SUCCEEDED;
	r3->data_length = sizeof(data2);
	r3->data = (unsigned char *)calloc(r3->data_length, sizeof(unsigned char));
	memcpy(r3->data, data2, r3->data_length);
	deps->expected_responses = list_append(deps->expected_responses, r3);

	/* let's add some requests to the request tracker IDs 0,1 and 2 */
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 0, 3, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 1, 3, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 2, 3, NULL);

	*state = deps;

	return 0;
}

/* runs once after all tests */
static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, &fake_handle);
	usbi3c_free_responses(&deps->expected_responses);
	FREE(deps);

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *node = NULL;

	/* remove any response we could have received during the test */
	for (node = deps->usbi3c_dev->request_tracker->regular_requests->requests; node; node = node->next) {
		bulk_transfer_free_response(&((struct regular_request *)node->data)->response);
	}

	return 0;
}

/* Negative test to verify the function handles receiving an unknown response gracefully.
 *
 * An unknown response is such that its request ID doesn't match any request  in the request
 * tracker, meaning the user did not send it. */
static void test_negative_getting_an_unknown_response(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;

	response_buffer_size = helper_create_multiple_response_buffer(&response_buffer, deps->expected_responses, 0);

	/* let's change the value of one pf the request id in the request_tracker
	 * so it doesn't match the response we are getting */
	((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->request_id = 100;

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* since we changed the second record in the tracker the first record
	 * should have a response, but the second and third should not, since
	 * the process should have aborted when the second ID was not found */
	assert_non_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->response);
	assert_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->response);
	assert_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->next->data)->response);

	((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->request_id = 1;
	free(response_buffer);
}

/* Negative test to verify the function handles receiving an already existing response gracefully.
 *
 * An existing response is such that a response is already in the request tracker */
static void test_negative_getting_an_existing_response(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response *resp = NULL;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;

	response_buffer_size = helper_create_multiple_response_buffer(&response_buffer, deps->expected_responses, 0);

	/* add a response to one request in the tracker so it already has one
	 * when we receive another response for the same request */
	resp = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	resp->attempted = USBI3C_COMMAND_ATTEMPTED;
	resp->has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	resp->error_status = USBI3C_SUCCEEDED;
	resp->data_length = 0;
	resp->data = NULL;
	((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->response = resp;

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* we added a response to the second record in the tracker, this should
	 * have caused the process to abort due to the invalid/repeated ID, so
	 * we should have a response for the first two records */
	assert_non_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->response);
	assert_non_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->response);
	assert_null(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->next->data)->response);

	free(response_buffer);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to receive data
 *  - Verifies the function is able to parse the list of responses and corresponding data from the buffer
 *  - Verifies the function succeeds on receiving the response (using a mock transfer),
 *    and that returns the expected value
 */
static void test_receiving_list_of_command_responses(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *regular_request = NULL;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;
	int id;

	response_buffer_size = helper_create_multiple_response_buffer(&response_buffer, deps->expected_responses, 0);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* we should have received 3 responses, one per request */

	/* check the first response */
	id = 0;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, 0);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_int_equal(regular_request->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(regular_request->response->data_length, 35);
	assert_memory_equal(regular_request->response->data, "Some test data with a length of 35", 35);

	/* check the second response */
	id = 1;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, 1);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(regular_request->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(regular_request->response->data_length, 0);
	assert_null(regular_request->response->data);

	/* check the third response */
	id = 2;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, 2);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_int_equal(regular_request->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(regular_request->response->data_length, 10);
	assert_memory_equal(regular_request->response->data, "Some data", 10);

	free(response_buffer);
}

/* This test validates the following:
 *  - Verifies the function is able to parse the list of responses and corresponding data from the buffer
 *    when one of the commands have no response descriptor.
 */
static void test_receiving_list_of_command_responses_with_failed_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *regular_request = NULL;
	unsigned char *response_buffer = NULL;
	int response_buffer_size = 0;
	int id;

	/* If one command in a list of commands fails execution,
	 * and the error handling is set to terminate on error,
	 * will force all subsequent commands to be aborted.
	 * These commands won't have a response descriptor.*/
	response_buffer_size = create_mock_response_with_failure(&response_buffer, deps->usbi3c_dev->request_tracker);
	assert_non_null(response_buffer);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/* we should have received 4 responses, one per request, 1 failed, 2 not attempted */

	/* check the first response */
	id = 3;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, id);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(regular_request->response->data_length, 51);
	assert_int_equal(regular_request->response->error_status, USBI3C_SUCCEEDED);
	assert_memory_equal(regular_request->response->data, "Some arbitrary test data with a length of 51 bytes", 51);

	/* check the second response */
	id = 4;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, id);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(regular_request->response->data_length, 0);
	assert_int_equal(regular_request->response->error_status, USBI3C_FAILED_FRAME_ERROR);
	assert_null(regular_request->response->data);

	/* check the third response */
	id = 5;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, id);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_NOT_ATTEMPTED);

	/* check the fourth response */
	id = 6;
	regular_request = (struct regular_request *)list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->request_id, id);
	assert_int_equal(regular_request->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(regular_request->response->attempted, USBI3C_COMMAND_NOT_ATTEMPTED);

	free(response_buffer);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_get_response() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_negative_getting_an_unknown_response, test_teardown),
		cmocka_unit_test_teardown(test_negative_getting_an_existing_response, test_teardown),
		cmocka_unit_test_teardown(test_receiving_list_of_command_responses, test_teardown),
		cmocka_unit_test_teardown(test_receiving_list_of_command_responses_with_failed_command, test_teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
