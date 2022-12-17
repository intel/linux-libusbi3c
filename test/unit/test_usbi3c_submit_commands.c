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

struct list *create_test_commands(void);
int response_cb(struct usbi3c_response *response, void *user_data);

int fake_handle = 1;

const int DEVICE_ADDRESS = 1;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);
	helper_initialize_controller(deps->usbi3c_dev, &fake_handle, NULL);

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

static void free_regular_request(struct regular_request **request)
{
	if (*request == NULL) {
		return;
	}
	if ((*request)->response) {
		bulk_transfer_free_response(&(*request)->response);
	}
	FREE(*request);
}

static void free_regular_request_in_list(void *data)
{
	struct regular_request *request = (struct regular_request *)data;

	free_regular_request(&request);
}

static int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	list_free_list_and_data(&deps->usbi3c_dev->request_tracker->regular_requests->requests, free_regular_request_in_list);

	return 0;
}

int response_cb(struct usbi3c_response *response, void *user_data)
{
	int *callback_called = (int *)user_data;

	/* make sure the response we got is accurate */
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	if (response->has_data) {
		if (response->data_length == 35) {
			assert_memory_equal(response->data, "Some test data with a length of 35", 35);
		} else if (response->data_length == 10) {
			assert_memory_equal(response->data, "Some data", 10);
		}
	}

	/* let's change the global flag */
	*callback_called += 1;

	return 0;
}

int bad_response_cb(struct usbi3c_response *response, void *user_data)
{
	int *callback_called = (int *)user_data;

	/* let's change the global flag */
	*callback_called = 1;

	return 1;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	(void)state; /* unused */
	int ret = 0;

	ret = usbi3c_submit_commands(NULL, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a missing list gracefully */
static void test_negative_missing_list_of_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a missing command gracefully */
static void test_negative_missing_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* let's add an invalid node to the command queue (missing command) */
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, NULL);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, -1);
}

/* Negative test to verify the function handles a missing callback gracefully,
 * since the execution of the command will be async a callback is necessary, so the
 * command execution should be aborted */
static void test_negative_missing_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int ret = 0;

	/* let's manually add a command without callback to the command queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;
	command->on_response_cb = NULL;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, -1);
}

/* Negative test to verify that if the commands fail to be submitted nothing gets added to the tracker */
static void test_negative_submit_failed(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int buffer_available;
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int ret = 0;

	/* let's manually add a valid command to the command queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;
	command->on_response_cb = response_cb;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* get a representation of how the command would look in memory, along with the size it would require */
	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, USBI3C_RESPONSE_HAS_NO_DATA, NULL, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	buffer_available = expected_command_buffer_size + 100;
	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_FAILURE);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, -1);

	/* verify the requests were not added to the tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	free(expected_command_buffer);
}

/* Negative test to verify that when a a command is submitted, and the callback fails, the response is kept in the tracker */
static void test_negative_callback_fails(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int buffer_available = 0;
	int response_buffer_size = 0;
	unsigned char *response_buffer = NULL;
	struct usbi3c_response expected_response;
	unsigned char data[] = "Some test data with a length of 35";
	struct regular_request *regular_request = NULL;
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int callback_called = 0;
	int ret;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's arbitrarily simulate we have a buffer available that
	 * is larger by 100 bytes than what we need */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* let's manually add a valid command to the command queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data);
	command->data = (unsigned char *)calloc(1, sizeof(data));
	memcpy(command->data, data, sizeof(data));

	/* let's add a callback function that is going to fail */
	command->on_response_cb = bad_response_cb;
	command->user_data = (void *)&callback_called;

	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* verify the callback function was correctly assigned */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests->data);
	assert_ptr_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->on_response_cb, &bad_response_cb);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" async from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = data;
	expected_response.data_length = sizeof(data);
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, request_id);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/*****************************/
	/* Post-response validations */
	/*****************************/

	/* make sure the callback we provided was run */
	assert_true(callback_called);

	/* since the callback returned a non-zero code, the request should not be removed from the
	 * tracker and the response should have been attached to it */
	regular_request = list_search(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id);
	assert_non_null(regular_request);
	assert_int_equal(regular_request->response->attempted, expected_response.attempted);
	assert_int_equal(regular_request->response->has_data, expected_response.has_data);
	assert_int_equal(regular_request->response->data_length, expected_response.data_length);
	assert_int_equal(regular_request->response->error_status, expected_response.error_status);
	assert_memory_equal(regular_request->response->data, expected_response.data, sizeof(data));

	/******************/
	/* Free resources */
	/******************/

	free(response_buffer);
	free(expected_command_buffer);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to send data
 *  - Verifies the function creates the correct data buffer that includes the command descriptor,
 *    appended data, and callback function
 *  - Verifies the function the function succeeds on sending the command (using a mock transfer),
 *    and returns the expected value
 */
static void test_submitting_single_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int buffer_available = 0;
	int response_buffer_size = 0;
	unsigned char *response_buffer = NULL;
	struct usbi3c_response expected_response;
	unsigned char data[] = "Some test data with a length of 35";
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int callback_called = 0;
	int ret;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's arbitrarily simulate we have a buffer available that
	 * is larger by 100 bytes than what we need */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* let's manually add a valid command to the command queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data);
	command->data = (unsigned char *)calloc(1, sizeof(data));
	memcpy(command->data, data, sizeof(data));
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;

	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* verify the callback function was correctly assigned */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests->data);
	assert_ptr_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->on_response_cb, &response_cb);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" async from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = data;
	expected_response.data_length = sizeof(data);
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, request_id);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/*****************************/
	/* Post-response validations */
	/*****************************/

	/* make sure the callback we provided was run */
	assert_true(callback_called);

	/* let's check that the command is no longer being tracked */
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));

	/******************/
	/* Free resources */
	/******************/

	free(expected_command_buffer);
	free(response_buffer);
}

/* This test validates that commands can be submitted at different I3C mode and rates */
static void test_submitting_command_at_different_transfer_mode(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int buffer_available = 0;
	int response_buffer_size = 0;
	unsigned char *response_buffer = NULL;
	struct usbi3c_response expected_response;
	unsigned char data[] = "Some test data with a length of 35";
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int callback_called = 0;
	int ret;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, USBI3C_I3C_HDR_DDR_MODE, USBI3C_I3C_RATE_6_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's arbitrarily simulate we have a buffer available that
	 * is larger by 100 bytes than what we need */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* by default the I3C transfer mode is SDR mode, and the transfer rate is 2 MHZ,
	 * so for this test we need to change those values to use:
	 * Mode: HDR DDR
	 * Rate: 6 MHZ */
	usbi3c_set_i3c_mode(deps->usbi3c_dev, USBI3C_I3C_HDR_DDR_MODE, USBI3C_I3C_RATE_6_MHZ, 0);

	/* NOTE: For this one test we'll use the usbi3c_enqueue_command() function since that is the
	 * one that assigns the mode and rate we setup in the context. */
	usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, response_cb, (void *)&callback_called);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* verify the callback function was correctly assigned */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests->data);
	assert_ptr_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->on_response_cb, &response_cb);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" async from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = data;
	expected_response.data_length = sizeof(data);
	response_buffer_size = helper_create_response_buffer(&response_buffer, &expected_response, request_id);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/*****************************/
	/* Post-response validations */
	/*****************************/

	/* make sure the callback we provided was run */
	assert_true(callback_called);

	/* let's check that the command is no longer being tracked */
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));

	/******************/
	/* Free resources */
	/******************/

	free(expected_command_buffer);
	free(response_buffer);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to send data
 *  - Verifies the function creates the correct data buffer that includes all commands and appended data
 *  - Verifies the function the function succeeds on sending the list of commands (using a mock transfer),
 *    and returns the expected value
 *  - The commands should be added to the tracker along with their callback
 */
static void test_submit_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	struct list *node = NULL;
	int buffer_available = 0;
	int response_buffer_size = 0;
	unsigned char *response_buffer = NULL;
	struct list *expected_responses = NULL;
	struct usbi3c_response r1, r2, r3;
	unsigned char data1[] = "Some test data with a length of 35";
	unsigned char data2[] = "Some data";
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int callback_called = 0;
	int ret = -1;
	const int BYTES_TO_READ = 36; // has to be a multiple of 4 (32-bit aligned)

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data1), data1, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_to_command_buffer(request_id + 1, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL);
	expected_command_buffer_size = helper_add_to_command_buffer(request_id + 2, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data2), data2);

	/* let's arbitrarily simulate we have a buffer available that
	 * is larger by 100 bytes than what we need */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* let's manually add a valid command to the command queue */
	/* command 1 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data1);
	command->data = (unsigned char *)calloc(1, sizeof(data1));
	memcpy(command->data, data1, sizeof(data1));
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* command 2 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = BYTES_TO_READ;
	command->data = NULL;
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* command 3 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data2);
	command->data = (unsigned char *)calloc(1, sizeof(data2));
	memcpy(command->data, data2, sizeof(data2));
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* verify the callback function was correctly assigned */
	int count = 0;
	for (node = deps->usbi3c_dev->request_tracker->regular_requests->requests; node; node = node->next) {
		struct regular_request *req = (struct regular_request *)node->data;
		assert_non_null(req);
		assert_ptr_equal(req->on_response_cb, &response_cb);
		if (count == 0) {
			assert_int_equal(req->dependent_on_previous, FALSE);
		} else {
			assert_int_equal(req->dependent_on_previous, TRUE);
		}
		count++;
	}

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock responses that are going to be "received" async from the I3C device */
	r1.attempted = USBI3C_COMMAND_ATTEMPTED;
	r1.has_data = USBI3C_RESPONSE_HAS_DATA;
	r1.error_status = USBI3C_SUCCEEDED;
	r1.data = data1;
	r1.data_length = sizeof(data1);
	expected_responses = list_append(expected_responses, &r1);

	r2.attempted = USBI3C_COMMAND_ATTEMPTED;
	r2.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r2.error_status = USBI3C_SUCCEEDED;
	r2.data = NULL;
	r2.data_length = 0;
	expected_responses = list_append(expected_responses, &r2);

	r3.attempted = USBI3C_COMMAND_ATTEMPTED;
	r3.has_data = USBI3C_RESPONSE_HAS_DATA;
	r3.error_status = USBI3C_SUCCEEDED;
	r3.data = data2;
	r3.data_length = sizeof(data2);
	expected_responses = list_append(expected_responses, &r3);

	response_buffer_size = helper_create_multiple_response_buffer(&response_buffer, expected_responses, request_id);

	/* simulate a response received from the I3c function */
	helper_trigger_response(response_buffer, response_buffer_size);

	/*****************************/
	/* Post-response validations */
	/*****************************/

	/* make sure the callback we provided was run */
	assert_int_equal(callback_called, 3);

	/* let's check that the commands are no longer being tracked */
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));
	request_id++;
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));
	request_id++;
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));

	/******************/
	/* Free resources */
	/******************/

	list_free_list(&expected_responses);
	free(expected_command_buffer);
	free(response_buffer);
}

/* Test to validate the command dependency on previous requests gets set correctly */
static void test_submit_dependent_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	struct list *node = NULL;
	int buffer_available = 0;
	unsigned char *response_buffer = NULL;
	struct list *expected_responses = NULL;
	unsigned char data1[] = "Some test data with a length of 35";
	unsigned char data2[] = "Some data";
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int callback_called = 0;
	int ret = -1;
	const int BYTES_TO_READ = 36; // has to be a multiple of 4 (32-bit aligned)

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data1), data1, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_to_command_buffer(request_id + 1, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL);
	expected_command_buffer_size = helper_add_to_command_buffer(request_id + 2, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data2), data2);

	/* let's arbitrarily simulate we have a buffer available that
	 * is larger by 100 bytes than what we need */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* let's manually add a valid command to the command queue */
	/* command 1 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data1);
	command->data = (unsigned char *)calloc(1, sizeof(data1));
	memcpy(command->data, data1, sizeof(data1));
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* command 2 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = BYTES_TO_READ;
	command->data = NULL;
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* command 3 */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data2);
	command->data = (unsigned char *)calloc(1, sizeof(data2));
	memcpy(command->data, data2, sizeof(data2));
	command->on_response_cb = response_cb;
	command->user_data = (void *)&callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* requests have the right dependency value */
	int count = 0;
	for (node = deps->usbi3c_dev->request_tracker->regular_requests->requests; node; node = node->next) {
		struct regular_request *req = (struct regular_request *)node->data;
		assert_non_null(req);
		if (count == 0) {
			assert_int_equal(req->dependent_on_previous, TRUE);
		} else {
			assert_int_equal(req->dependent_on_previous, TRUE);
		}
		count++;
	}
	/* NOTE: we don't need to do extra validations for this test, those are already being validated in previous tests */

	/******************/
	/* Free resources */
	/******************/

	list_free_list(&expected_responses);
	free(expected_command_buffer);
	free(response_buffer);
}

/* Test to validate the target reset pattern can be submitted successfully */
static void test_submit_target_reset_pattern(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int request_id = bulk_request_id;
	int buffer_available = 0;
	struct usbi3c_response r1, r2;
	struct list *expected_responses = NULL;
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;

	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	const int CCC_RSTACT_BROADCAST = 0x2A;
	const int RESET_WHOLE_TARGET = 0x02;

	/* variables required for validation */
	int callback_called = 0;
	struct list *node = NULL;
	int ret = -1;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_with_defining_byte_buffer(request_id,
										   CCC_RSTACT_BROADCAST,
										   RESET_WHOLE_TARGET,
										   &expected_command_buffer,
										   USBI3C_BROADCAST_ADDRESS,
										   USBI3C_WRITE,
										   USBI3C_TERMINATE_ON_ANY_ERROR,
										   USBI3C_RESPONSE_HAS_NO_DATA,
										   NULL,
										   USBI3C_I3C_SDR_MODE,
										   USBI3C_I3C_RATE_2_MHZ,
										   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_target_reset_pattern_to_command_buffer(request_id + 1, &expected_command_buffer, expected_command_buffer_size);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* first command: a RSTACT CCC to configure which Targets are to be reset,
+        * the level(s) of reset to be used, and which Targets are not to be reset */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	command->command_descriptor->common_command_code = CCC_RSTACT_BROADCAST;
	command->command_descriptor->defining_byte = RESET_WHOLE_TARGET;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->target_address = USBI3C_BROADCAST_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->data_length = USBI3C_RESPONSE_HAS_NO_DATA;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->data = NULL;
	/* to submit a command we need to provide a callback */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* second command: the target reset pattern */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = TARGET_RESET_PATTERN;
	/* to submit a command we need to provide a callback */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/****************************/
	/* Pre-response validations */
	/****************************/

	/* verify the callback function was correctly assigned */
	int count = 0;
	for (node = deps->usbi3c_dev->request_tracker->regular_requests->requests; node; node = node->next) {
		struct regular_request *req = (struct regular_request *)node->data;
		assert_non_null(req);
		assert_ptr_equal(req->on_response_cb, &response_cb);
		if (count == 0) {
			assert_int_equal(req->dependent_on_previous, FALSE);
		} else {
			assert_int_equal(req->dependent_on_previous, TRUE);
		}
		count++;
	}

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock responses that are going to be "received" async from the I3C device */
	r1.attempted = USBI3C_COMMAND_ATTEMPTED;
	r1.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r1.error_status = USBI3C_SUCCEEDED;
	r1.data = NULL;
	r1.data_length = 0;
	expected_responses = list_append(expected_responses, &r1);

	r2.attempted = USBI3C_COMMAND_ATTEMPTED;
	r2.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r2.error_status = USBI3C_SUCCEEDED;
	r2.data = NULL;
	r2.data_length = 0;
	expected_responses = list_append(expected_responses, &r2);

	expected_response_buffer_size = helper_create_multiple_response_buffer(&expected_response_buffer, expected_responses, request_id);

	/* simulate a response received from the I3c function */
	helper_trigger_response(expected_response_buffer, expected_response_buffer_size);

	/*****************************/
	/* Post-response validations */
	/*****************************/

	/* make sure the callback we provided was run */
	assert_int_equal(callback_called, 2);

	/* let's check that the commands are no longer being tracked */
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));
	request_id++;
	assert_null(list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &request_id, compare_request_id));
	request_id++;

	/******************/
	/* Free resources */
	/******************/

	list_free_list(&expected_responses);
	free(expected_response_buffer);
	free(expected_command_buffer);
}

int main(void)
{

	/* Unit tests for the usbi3c_submit_commands() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_negative_missing_context, teardown),
		cmocka_unit_test_teardown(test_negative_missing_list_of_commands, teardown),
		cmocka_unit_test_teardown(test_negative_missing_command, teardown),
		cmocka_unit_test_teardown(test_negative_missing_callback, teardown),
		cmocka_unit_test_teardown(test_negative_submit_failed, teardown),
		cmocka_unit_test_teardown(test_negative_callback_fails, teardown),
		cmocka_unit_test_teardown(test_submitting_single_command, teardown),
		cmocka_unit_test_teardown(test_submitting_command_at_different_transfer_mode, teardown),
		cmocka_unit_test_teardown(test_submit_commands, teardown),
		cmocka_unit_test_teardown(test_submit_dependent_commands, teardown),
		// TODO: add a test to submit a CCC command
		cmocka_unit_test_teardown(test_submit_target_reset_pattern, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
