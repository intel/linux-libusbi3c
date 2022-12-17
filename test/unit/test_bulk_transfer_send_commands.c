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
	struct list *commands;
	unsigned char *buffer;
	int buffer_size;
	int request_id;
};

/* runs once before all tests */
static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	/* get a usbi3c context */
	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);
	deps->commands = NULL;

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

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	bulk_transfer_free_commands(&deps->commands);
	free(deps->buffer);

	return 0;
}

/* Negative test to verify the function handles a bad dependency value gracefully */
static void test_negative_invalid_dependency_value(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *request_ids = NULL;
	const int INVALID_VALUE = 2;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, INVALID_VALUE);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, INVALID_VALUE);
	assert_null(request_ids);
}

/* Negative test to verify that the function handles a failure with libusb getting the buffer size gracefully */
static void test_negative_libusb_failure_getting_buffer_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *request_ids = NULL;
	int buffer_available = 0;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = deps->buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_FAILURE);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_null(request_ids);

	/* let's validate that the IDs are not leftover in the request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
}

/* Negative test to verify that if sending the commands fail, and the tracker was empty,
 * the tracker remains empty */
static void test_negative_sending_commands_fails_with_empty_tracker(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *request_ids = NULL;
	int buffer_available = 0;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = deps->buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(deps->buffer, deps->buffer_size, RETURN_FAILURE);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_null(request_ids);

	/* let's validate that the IDs are not leftover in the request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
}

/* Negative test to verify that if sending the commands fail, and the tracker was not empty,
 * the tracker keeps only the original requests */
static void test_negative_sending_commands_fails_with_non_empty_tracker(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *request_ids = NULL;
	int buffer_available = 0;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = deps->buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(deps->buffer, deps->buffer_size, RETURN_FAILURE);

	/* let's add some existing requests to the tracker */
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 100, 1, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 101, 2, NULL);
	helper_add_request_to_tracker(deps->usbi3c_dev->request_tracker, 102, 2, NULL);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_null(request_ids);

	/* let's validate that the IDs are not leftover in the request tracker */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
	assert_int_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data)->request_id, 100);
	assert_int_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->data)->request_id, 101);
	assert_int_equal(((struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->next->next->data)->request_id, 102);
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests->next->next->next);
}

/* test for the bulk_transfer_send_commands() function */
static void test_bulk_transfer_send_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *regular_request = NULL;
	struct list *request_ids = NULL;
	struct list *node = NULL;
	int initial_id = bulk_request_id;
	int buffer_available = 0;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = deps->buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(deps->buffer, deps->buffer_size, RETURN_SUCCESS);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_non_null(request_ids);

	/* let's validate that a record for each command was created correctly in the regular
	 * request tracker, it should not have a response yet */
	node = list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &initial_id, compare_request_id);
	regular_request = (struct regular_request *)node->data;
	assert_int_equal(regular_request->request_id, initial_id);
	assert_int_equal(regular_request->total_commands, 3);
	assert_null(regular_request->response);
	/* all commands in the same bulk request transfer are dependent on the previous
	 * by default, this is the first command from the request, so this one should
	 * have the dependent_on_previous set to FALSE */
	assert_int_equal(regular_request->dependent_on_previous, FALSE);

	regular_request = (struct regular_request *)node->next->data;
	assert_int_equal(regular_request->request_id, initial_id + 1);
	assert_int_equal(regular_request->total_commands, 3);
	assert_null(regular_request->response);
	/* second command in the request, depends on the first */
	assert_int_equal(regular_request->dependent_on_previous, TRUE);

	regular_request = (struct regular_request *)node->next->next->data;
	assert_int_equal(regular_request->request_id, initial_id + 2);
	assert_int_equal(regular_request->total_commands, 3);
	assert_null(regular_request->response);
	/* third command in the request, depends on the second */
	assert_int_equal(regular_request->dependent_on_previous, TRUE);

	/* validate that only three records were added to the tracker */
	assert_null(node->next->next->next);

	list_free_list_and_data(&request_ids, free);
}

/* test for the bulk_transfer_send_commands() function to make sure commands that depend on previous can be sent */
static void test_bulk_transfer_send_dependent_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *regular_request = NULL;
	struct list *request_ids = NULL;
	struct list *node = NULL;
	int initial_id = bulk_request_id;
	int buffer_available = 0;

	deps->commands = helper_create_commands(response_cb, NULL, &deps->buffer, &deps->buffer_size, &deps->request_id, USBI3C_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = deps->buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(deps->buffer, deps->buffer_size, RETURN_SUCCESS);

	request_ids = bulk_transfer_send_commands(deps->usbi3c_dev, deps->commands, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_non_null(request_ids);

	/* let's validate that the request dependency is set correctly on the tracker */
	node = list_search_node(deps->usbi3c_dev->request_tracker->regular_requests->requests, &initial_id, compare_request_id);
	regular_request = (struct regular_request *)node->data;
	assert_int_equal(regular_request->request_id, initial_id);
	assert_int_equal(regular_request->total_commands, 3);
	/* all commands in the same bulk request transfer are dependent on the previous
	 * by default, but the first command from the request can be independent (default)
	 * or dependent based on the option chosen by the user when the bulk request was
	 * submitted. If set to depend on the previous it would mean all commands in these
	 * request depend on the commands from the previous request */
	assert_int_equal(regular_request->dependent_on_previous, TRUE);

	regular_request = (struct regular_request *)node->next->data;
	assert_int_equal(regular_request->request_id, initial_id + 1);
	assert_int_equal(regular_request->total_commands, 3);
	assert_null(regular_request->response);
	/* second command in the request, depends on the first */
	assert_int_equal(regular_request->dependent_on_previous, TRUE);

	regular_request = (struct regular_request *)node->next->next->data;
	assert_int_equal(regular_request->request_id, initial_id + 2);
	assert_int_equal(regular_request->total_commands, 3);
	assert_null(regular_request->response);
	/* third command in the request, depends on the second */
	assert_int_equal(regular_request->dependent_on_previous, TRUE);

	list_free_list_and_data(&request_ids, free);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_send_commands() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_negative_invalid_dependency_value, test_teardown),
		cmocka_unit_test_teardown(test_negative_libusb_failure_getting_buffer_size, test_teardown),
		cmocka_unit_test_teardown(test_negative_sending_commands_fails_with_empty_tracker, test_teardown),
		cmocka_unit_test_teardown(test_negative_sending_commands_fails_with_non_empty_tracker, test_teardown),
		cmocka_unit_test_teardown(test_bulk_transfer_send_commands, test_teardown),
		cmocka_unit_test_teardown(test_bulk_transfer_send_dependent_commands, test_teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
