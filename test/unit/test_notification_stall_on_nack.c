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

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

/* Negative test to validate that the function handles receiving a "stall on nack" notification
 * that contains a request ID that is not in the request tracker gracefully */
static void test_negative_stalled_request_not_in_tracker(void **state)
{
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	const int REQUEST_ID = 1;

	/* let's simulate a "Stall on Nack" notification coming from the I3C function,
	 * that indicates the request id 1 got stalled */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_STALL_ON_NACK, REQUEST_ID);
	helper_trigger_notification(buffer, buffer_size);

	free(buffer);
}

/* Test to validate that maximum number of times a request can be resumed can be changed by the user */
static void test_change_default_reattempt_max(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	/* validate the default reattempts is 2 */
	assert_int_equal(deps->usbi3c_dev->request_tracker->reattempt_max, 2);

	usbi3c_set_request_reattempt_max(deps->usbi3c_dev, 10);
	/* after running the function the new mac value should be accurate */
	assert_int_equal(deps->usbi3c_dev->request_tracker->reattempt_max, 10);
}

/* Test to validate that when a "stall on nack" notification is received, the stall command
 * is reattempted automatically */
static void test_stalled_request_reattempted(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	const int REQUEST_ID = 1;

	/* let's add a command to the request tracker to simulate a command that
	 * was previously sent and stalled the I3C controller */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = REQUEST_ID;
	request->total_commands = 1;
	request->dependent_on_previous = FALSE;
	request->reattempt_count = 1;
	requests = list_append(requests, request);
	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* let's simulate a "Stall on Nack" notification coming from the I3C function,
	 * that indicates the request with id 1 got stalled. The notification should trigger
	 * the notification handler which will run the bulk_transfer_resume_request_async()
	 * function to retry the stalled command */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_STALL_ON_NACK, REQUEST_ID);
	helper_trigger_notification(buffer, buffer_size);

	/* the bulk_transfer_resume_request_async() function is async, so we'll need to mock
	 * it along with an event */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the request should still be in the tracker */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
	request = (struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data;
	assert_int_equal(request->reattempt_count, 2);
	assert_int_equal(request->request_id, REQUEST_ID);

	free(buffer);
}

/* Test to validate that when a "stall on nack" notification is received, the stall command
 * is reattempted automatically, if it keeps stalling and reaches the max number of reattempts
 * then the request is canceled */
static void test_stalled_request_canceled(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	const int REQUEST_ID = 1;
	const int REATTEMPT_MAX = 2;

	/* let's add a command to the request tracker to simulate a stalled request that
	 * has been already reattempted the max number of times and should next be cancelled */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = REQUEST_ID;
	request->total_commands = 1;
	request->dependent_on_previous = FALSE;
	request->reattempt_count = REATTEMPT_MAX;
	requests = list_append(requests, request);
	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* let's simulate another "Stall on Nack" notification coming from the I3C function,
	 * that indicates the request id 1 got stalled once more. The notification should trigger
	 * the notification handler which will run the bulk_transfer_cancel_request_async() function
	 * that will cancel the stalled command */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_STALL_ON_NACK, REQUEST_ID);
	helper_trigger_notification(buffer, buffer_size);

	/* the bulk_transfer_cancel_request_async() function is async,
	 * so we'll need to mock it along with an event */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the request should have been removed from the tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	free(buffer);
}

/* Test to validate that when a "stall on nack" notification is received, and the request is canceled,
 * all requests that depend on it get cancelled too */
static void test_stalled_request_canceled_with_dependent(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	const int REQUEST_ID = 1;
	const int REATTEMPT_MAX = 2;

	/* let's add some commands to the request tracker to
	 * simulate a couple of requests being already transferred:
	 * - the first request with three commands. IDs: 0, 1, 2
	 * - the second request with three commands (dependent on previous request). IDs: 3, 4, 5 */

	/* Request 1 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 0;
	request->total_commands = 3;
	request->dependent_on_previous = FALSE;
	request->reattempt_count = 0;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 1;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	request->reattempt_count = REATTEMPT_MAX;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 2;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	request->reattempt_count = 0;
	requests = list_append(requests, request);

	/* Request 2 */
	/* First command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 3;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	request->reattempt_count = 0;
	requests = list_append(requests, request);
	/* Second command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 4;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	request->reattempt_count = 0;
	requests = list_append(requests, request);
	/* Third command */
	request = (struct regular_request *)calloc(1, sizeof(struct regular_request));
	request->request_id = 5;
	request->total_commands = 3;
	request->dependent_on_previous = TRUE;
	request->reattempt_count = 0;
	requests = list_append(requests, request);

	deps->usbi3c_dev->request_tracker->regular_requests->requests = requests;

	/* let's simulate another "Stall on Nack" notification coming from the I3C function,
	 * that indicates the request id 1 got stalled once more. The notification should trigger
	 * the notification handler which will run the bulk_transfer_cancel_request_async() function
	 * that will cancel the stalled command along with all the commands that depend on it */
	buffer_size = helper_create_notification_buffer(&buffer, NOTIFICATION_STALL_ON_NACK, REQUEST_ID);
	helper_trigger_notification(buffer, buffer_size);

	/* the bulk_transfer_cancel_request_async() function is async,
	 * so we'll need to mock it along with an event */
	mock_cancel_or_resume_bulk_request(RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usbi3c_dev->usb_dev);

	/* the request should have been removed from the tracker along with requests that depend on it,
	 * so only one request (request 0) should still be in the tracker */
	assert_non_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests->next);
	request = (struct regular_request *)deps->usbi3c_dev->request_tracker->regular_requests->requests->data;
	assert_int_equal(request->request_id, 0);
	assert_int_equal(request->reattempt_count, 0);

	free(buffer);
}

int main(void)
{
	/* Unit tests for the "Stall on Nack" notification handler */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_stalled_request_not_in_tracker, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_change_default_reattempt_max, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_stalled_request_reattempted, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_stalled_request_canceled, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_stalled_request_canceled_with_dependent, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
