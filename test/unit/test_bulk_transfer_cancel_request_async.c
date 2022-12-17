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

int fake_handle = 1;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(&fake_handle);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
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

/* Negative test to verify that the function handles a missing usb session gracefully */
static void test_negative_missing_usb_session(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int REQUEST_ID = 0;

	ret = bulk_transfer_cancel_request_async(NULL, deps->usbi3c_dev->request_tracker->regular_requests, REQUEST_ID);
	assert_int_equal(ret, -1);
}

/* Negative test to verify that the function handles a missing request tracker gracefully */
static void test_negative_missing_request_tracker(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int REQUEST_ID = 0;

	ret = bulk_transfer_cancel_request_async(deps->usbi3c_dev->usb_dev, NULL, REQUEST_ID);
	assert_int_equal(ret, -1);
}

/* Test to verify that a request to cancel a stalled command can be submitted */
static void test_cancel_bulk_request(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int REQUEST_ID = 0;
	int ret = -1;

	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);

	ret = bulk_transfer_cancel_request_async(deps->usbi3c_dev->usb_dev, deps->usbi3c_dev->request_tracker->regular_requests, REQUEST_ID);
	assert_int_equal(ret, 0);

	/* bulk_transfer_cancel_request_async() is run asynchronously so we need to trigger
	 * a fake event about the transfer having completed */
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);
}

/* Test to verify that if a request to cancel a stalled command is sent, and the
 * command is part of a request transfer that included more commands, the commands
 * after the stalled command will also be cancelled, however commands from subsequent
 * requests will not */
static void test_cancel_bulk_request_without_dependent_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	int req_id;
	int ret = -1;

	/* let's add some commands to the request tracker to
	 * simulate a couple of requests being already transferred:
	 * - the first request with three commands
	 * - the second request with three commands (non-dependent on previous request) */

	/* Request 1 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 0;
	request->total_commands = 3;
	request->dependent_on_previous = FALSE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 1;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 2;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	/* Request 2 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 3;
	request->total_commands = 3;
	request->dependent_on_previous = FALSE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 4;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 5;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* mock the cancel request */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);

	/* let's say the command that caused the controller to stall is command with ID = 1 */
	const int STALLED_COMMAND_ID = 1;
	ret = bulk_transfer_cancel_request_async(deps->usbi3c_dev->usb_dev, deps->usbi3c_dev->request_tracker->regular_requests, STALLED_COMMAND_ID);
	assert_int_equal(ret, 0);

	/* bulk_transfer_cancel_request_async() is run asynchronously so we need to trigger
	 * a fake event about the transfer having completed */
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the stalled command and all subsequent commands from the first request
	 * should have been deleted from the request tracker */
	req_id = 0;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_non_null(request);
	req_id = STALLED_COMMAND_ID;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);
	req_id = STALLED_COMMAND_ID + 1;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);

	/* none of the commands from the second request should have been deleted
	 * from the request tracker */
	req_id = STALLED_COMMAND_ID + 2;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_non_null(request);
	req_id = STALLED_COMMAND_ID + 3;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_non_null(request);
	req_id = STALLED_COMMAND_ID + 4;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_non_null(request);

	list_free_list_and_data(&deps->usbi3c_dev->request_tracker->regular_requests->requests, free_regular_request_in_list);
}

/* Test to verify that if a request to cancel a stalled command is sent, and the
 * command is part of a request transfer, that other request transfers depend on,
 * all commands in those request transfers will also be cancelled */
static void test_cancel_bulk_request_with_dependent_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	int req_id;
	int ret = -1;

	/* let's add some commands to the request tracker to
	 * simulate a couple of requests being already transferred:
	 * - the first request with three commands
	 * - the second request with three commands (dependent on previous request) */

	/* Request 1 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 0;
	request->total_commands = 3;
	request->dependent_on_previous = FALSE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 1;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 2;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	/* Request 2 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 3;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 4;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 5;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* mock the cancel request */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);

	/* let's say the command that caused the controller to stall is command with ID = 1 */
	const int STALLED_COMMAND_ID = 1;
	ret = bulk_transfer_cancel_request_async(deps->usbi3c_dev->usb_dev, deps->usbi3c_dev->request_tracker->regular_requests, STALLED_COMMAND_ID);
	assert_int_equal(ret, 0);

	/* bulk_transfer_cancel_request_async() is run asynchronously so we need to trigger
	 * a fake event about the transfer having completed */
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the stalled command and all subsequent commands from the first request
	 * should have been deleted from the request tracker */
	req_id = 0;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_non_null(request);
	req_id = STALLED_COMMAND_ID;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);
	req_id = STALLED_COMMAND_ID + 1;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);

	/* all of the commands from the second request should have been deleted
	 * from the request tracker since they were dependent on the 1st request */
	req_id = STALLED_COMMAND_ID + 2;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);
	req_id = STALLED_COMMAND_ID + 3;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);
	req_id = STALLED_COMMAND_ID + 4;
	request = (struct regular_request *)list_search(requests, &req_id, compare_request_id);
	assert_null(request);

	list_free_list_and_data(&deps->usbi3c_dev->request_tracker->regular_requests->requests, free_regular_request_in_list);
}

/* Test to verify that if all requests in the tracker are dependent of the 1st
 * command, and that one gets cancelled, all commands get removed from the tracker */
static void test_cancel_bulk_request_all_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	int ret = -1;

	/* let's add some commands to the request tracker to
	 * simulate a couple of requests being already transferred:
	 * - the first request with three commands
	 * - the second request with three commands (dependent on previous request) */

	/* Request 1 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 0;
	request->total_commands = 3;
	request->dependent_on_previous = FALSE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 1;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 2;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	/* Request 2 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 3;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 4;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 5;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	requests = list_append(requests, request);

	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* mock the cancel request */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);

	/* let's say the command that caused the controller to stall is command with ID = 0 */
	const int STALLED_COMMAND_ID = 0;
	ret = bulk_transfer_cancel_request_async(deps->usbi3c_dev->usb_dev, deps->usbi3c_dev->request_tracker->regular_requests, STALLED_COMMAND_ID);
	assert_int_equal(ret, 0);

	/* bulk_transfer_cancel_request_async() is run asynchronously so we need to trigger
	 * a fake event about the transfer having completed */
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the stalled command and all subsequent commands from the first request
	 * should have been deleted from the request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
}

int main(void)
{

	/* Unit tests for the bulk_transfer_cancel_request_async() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_missing_usb_session, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_negative_missing_request_tracker, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_cancel_bulk_request, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_cancel_bulk_request_without_dependent_commands, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_cancel_bulk_request_with_dependent_commands, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_cancel_bulk_request_all_commands, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
