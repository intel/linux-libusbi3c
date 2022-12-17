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
	struct usbi3c_command *command;
	unsigned char *buffer;
	int buffer_size;
	int request_id;
};

/* runs once before all tests */
static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));
	struct usbi3c_response *response = NULL;
	struct regular_request *request = NULL;

	/* get a usbi3c context */
	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);
	deps->command = NULL;

	/* to test the function that searches for a response in the tracker,
	 * let's manually add a couple of responses to the request tracker */

	/* first request (with no response) */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 0;
	request->on_response_cb = NULL;
	request->total_commands = 1;
	request->response = NULL;
	deps->usbi3c_dev->request_tracker->regular_requests->requests = list_append(deps->usbi3c_dev->request_tracker->regular_requests->requests, request);

	/* second request (with response) */
	response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	response->attempted = USBI3C_COMMAND_ATTEMPTED;
	response->has_data = USBI3C_RESPONSE_HAS_DATA;
	response->data = (unsigned char *)calloc(11, sizeof(unsigned char));
	memcpy(response->data, "Test data!", 11);
	response->data_length = 11;
	response->error_status = USBI3C_SUCCEEDED;
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 1;
	request->on_response_cb = NULL;
	request->total_commands = 1;
	request->response = response;
	deps->usbi3c_dev->request_tracker->regular_requests->requests = list_append(deps->usbi3c_dev->request_tracker->regular_requests->requests, request);

	*state = deps;

	return 0;
}

/* runs once after all tests */
static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, &fake_handle);
	free(deps);

	return 0;
}

int response_cb(struct usbi3c_response *response, void *user_data)
{
	return response->error_status;
}

/* runs before every test */
static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	deps->command = helper_create_command(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id);

	return 0;
}

/* runs after every test */
static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	bulk_transfer_free_command(&deps->command);
	free(deps->buffer);

	return 0;
}

/* test for the bulk_transfer_search_response_in_tracker() function when the tracker is empty */
static void test_search_response_in_empty_tracker(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response *response = NULL;
	struct list *temp_list = NULL;
	int req_id = 1;

	/* remove all requests from the tracker */
	temp_list = deps->usbi3c_dev->request_tracker->regular_requests->requests;
	deps->usbi3c_dev->request_tracker->regular_requests->requests = NULL;

	response = bulk_transfer_search_response_in_tracker(deps->usbi3c_dev->request_tracker->regular_requests, req_id);
	assert_null(response);

	/* return the tracker so we don't loose it */
	deps->usbi3c_dev->request_tracker->regular_requests->requests = temp_list;
}

/* test for the bulk_transfer_search_response_in_tracker() function when the request id is not found */
static void test_search_response_in_tracker_request_not_found(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response *response = NULL;
	int req_id = 3;

	response = bulk_transfer_search_response_in_tracker(deps->usbi3c_dev->request_tracker->regular_requests, req_id);
	assert_null(response);

	if (response) {
		/* in case the test fails we need to free the resource */
		free(response);
	}
}

/* test for the bulk_transfer_search_response_in_tracker() function when the request is found but has no response */
static void test_search_response_in_tracker_response_not_found(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response *response = NULL;
	int req_id = 0;

	response = bulk_transfer_search_response_in_tracker(deps->usbi3c_dev->request_tracker->regular_requests, req_id);
	assert_null(response);

	if (response) {
		/* in case the test fails we need to free the resource */
		free(response);
	}
}

/* test for the bulk_transfer_search_response_in_tracker() function when the response is found */
static void test_search_response_in_tracker_response_found(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_response *response = NULL;
	struct list *node = NULL;
	int req_id = 1;

	response = bulk_transfer_search_response_in_tracker(deps->usbi3c_dev->request_tracker->regular_requests, req_id);
	assert_non_null(response);

	if (response) {
		assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
		assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_DATA);
		assert_int_equal(response->data_length, 11);
		assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
		assert_memory_equal(response->data, "Test data!", 11);
	}

	/* verify the request was removed from the tracker */
	for (node = deps->usbi3c_dev->request_tracker->regular_requests->requests; node; node = node->next) {
		struct regular_request *request = (struct regular_request *)node->data;
		assert_int_not_equal(request->request_id, req_id);
	}

	bulk_transfer_free_response(&response);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_search_response_in_tracker() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_search_response_in_empty_tracker, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_search_response_in_tracker_request_not_found, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_search_response_in_tracker_response_not_found, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_search_response_in_tracker_response_found, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
