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

const int RSTACT_BROADCAST = 0x2A;
const int RESET_WHOLE_TARGET = 0x02;
const int TIMEOUT = 60;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);

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

int on_response_cb(struct usbi3c_response *response, void *user_data)
{
	int *callback_called = (int *)user_data;

	*callback_called += 1;

	return response->error_status;
}

/* Verify that a reset action can be sent */
static void test_send_target_reset(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	int callback_called = 0;
	int ret = -1;

	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	unsigned char *expected_response_buffer = NULL;
	int expected_command_buffer_size = 0;
	int expected_response_buffer_size = 0;
	struct list *expected_responses = NULL;
	struct usbi3c_response r1, r2;
	int request_id = bulk_request_id;
	int buffer_available = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_with_defining_byte_buffer(request_id,
										   RSTACT_BROADCAST,
										   RESET_WHOLE_TARGET,
										   &expected_command_buffer,
										   USBI3C_BROADCAST_ADDRESS,
										   USBI3C_WRITE,
										   USBI3C_TERMINATE_ON_ANY_ERROR,
										   0,
										   NULL,
										   USBI3C_I3C_SDR_MODE,
										   USBI3C_I3C_RATE_2_MHZ,
										   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_target_reset_pattern_to_command_buffer(request_id + 1,
											 &expected_command_buffer,
											 expected_command_buffer_size);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(NULL, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock responses that are going to be "received" from the I3C device */
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

	expected_response_buffer_size = helper_create_multiple_response_buffer(&expected_response_buffer, expected_responses, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/* first a user would enqueue a ccc to configure the reset action to use */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev,
						    USBI3C_BROADCAST_ADDRESS,
						    USBI3C_WRITE,
						    USBI3C_TERMINATE_ON_ANY_ERROR,
						    RSTACT_BROADCAST,
						    RESET_WHOLE_TARGET,
						    0,
						    NULL,
						    on_response_cb,
						    &callback_called);
	assert_int_equal(ret, 0);

	/* then the user would enqueue a target reset pattern that will trigger the reset action */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, on_response_cb, &callback_called);
	assert_int_equal(ret, 0);

	/* send the commands in the queue for execution */
	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	usbi3c_free_responses(&responses);
	list_free_list(&expected_responses);
	free(expected_response_buffer);
	free(expected_command_buffer);
}

/* Verify that a reset action can be submitted to be run asynchronously */
static void test_submit_target_reset(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int callback_called = 0;
	int ret = -1;

	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	unsigned char *expected_response_buffer = NULL;
	int expected_command_buffer_size = 0;
	int expected_response_buffer_size = 0;
	struct list *expected_responses = NULL;
	struct usbi3c_response r1, r2;
	int request_id = bulk_request_id;
	int buffer_available = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_with_defining_byte_buffer(request_id,
										   RSTACT_BROADCAST,
										   RESET_WHOLE_TARGET,
										   &expected_command_buffer,
										   USBI3C_BROADCAST_ADDRESS,
										   USBI3C_WRITE,
										   USBI3C_TERMINATE_ON_ANY_ERROR,
										   0,
										   NULL,
										   USBI3C_I3C_SDR_MODE,
										   USBI3C_I3C_RATE_2_MHZ,
										   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_target_reset_pattern_to_command_buffer(request_id + 1,
											 &expected_command_buffer,
											 expected_command_buffer_size);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(NULL, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock responses that are going to be "received" from the I3C device */
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

	expected_response_buffer_size = helper_create_multiple_response_buffer(&expected_response_buffer, expected_responses, bulk_request_id);

	/* first a user would enqueue a ccc to configure the reset action to use */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev,
						    USBI3C_BROADCAST_ADDRESS,
						    USBI3C_WRITE,
						    USBI3C_TERMINATE_ON_ANY_ERROR,
						    RSTACT_BROADCAST,
						    RESET_WHOLE_TARGET,
						    0,
						    NULL,
						    on_response_cb,
						    &callback_called);
	assert_int_equal(ret, 0);

	/* then the user would enqueue a target reset pattern that will trigger the reset action */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, on_response_cb, &callback_called);
	assert_int_equal(ret, 0);

	/* send the commands in the queue for execution */
	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);

	/* simulate a response received from the I3c function */
	helper_trigger_response(expected_response_buffer, expected_response_buffer_size);

	/* the callback should have been called twice (once per executed command) */
	assert_int_equal(callback_called, 2);

	list_free_list(&expected_responses);
	free(expected_response_buffer);
	free(expected_command_buffer);
}

int main(void)
{
	/* Unit tests for the target reset functionality */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_send_target_reset),
		cmocka_unit_test(test_submit_target_reset),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
