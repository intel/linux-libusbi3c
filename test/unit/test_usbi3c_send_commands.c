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

const int TIMEOUT = 60;
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

int response_cb(struct usbi3c_response *response, void *user_data)
{
	int *callback_called = (int *)user_data;

	*callback_called = 1;

	return response->error_status;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context(void **state)
{
	(void)state; /* unused */
	struct list *responses = NULL;

	responses = usbi3c_send_commands(NULL, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);
}

/* Negative test to verify the function handles an empty command queue gracefully */
static void test_negative_empty_command_queue(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;

	/* before we begin, let's make sure the command queue is empty */
	assert_null(deps->usbi3c_dev->command_queue);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);
}

/* Negative test to verify the function handles a missing command gracefully */
static void test_negative_missing_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;

	/* let's add a node into the command queue but with a missing command */
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, NULL);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);
}

/* Negative test to verify the function handles a missing command descriptor gracefully
 * Note: Since the list of commands may depend on each other for their correct execution,
 * the whole list of commands should be dropped */
static void test_negative_missing_command_descriptor(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;

	/* let's manually add an empty command to the command queue so
	 * it won't have command descriptor */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);
}

/* Negative test to verify the function handles a missing data when the command requires it (data_length > 0) gracefully
 * Note: Since the list of commands may depend on each other for their correct execution,
 * the whole list of commands should be dropped */
static void test_negative_missing_command_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;

	/* let's add a command that is missing its data to the queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->data_length = 100;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);
}

/* Negative test to verify the function handles not having enough buffer available gracefully */
static void test_negative_not_enough_buffer_available(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	int buffer_available = 0;
	int expected_command_buffer_size = 0;
	unsigned char *expected_command_buffer = NULL;

	/* let's manually add a valid command to the command queue */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* get a representation of how the command would look in memory, along with the size it would require */
	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0, NULL, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is smaller than the required buffer by 2 bytes */
	buffer_available = expected_command_buffer_size - 2;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* this command should fail since there is not enough buffer available for the command */
	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_null(responses);

	free(expected_command_buffer);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to send data
 *  - Verifies the function creates the correct data buffer that includes the command descriptor
 *    and appended data
 *  - Verifies the function the function succeeds on sending the command (using a mock transfer),
 *    and returns the expected value
 *  - Verifies that a write command can be sent
 */
static void test_send_single_write_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char expected_response_data[] = "Response data";
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	unsigned char data[] = "Arbitrary test data";
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = expected_response_data;
	expected_response.data_length = sizeof(expected_response_data);
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data);
	command->data = (unsigned char *)calloc(1, sizeof(data));
	memcpy(command->data, data, sizeof(data));
	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_memory_equal(response->data, expected_response.data, sizeof(expected_response_data));

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test verifies that a read command can be sent */
static void test_send_single_read_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char expected_response_data[] = "Response data!!";
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	const int BYTES_TO_READ = 16; // multiple of 4 (32-bit aligned)
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = expected_response_data;
	expected_response.data_length = BYTES_TO_READ;
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = BYTES_TO_READ;
	command->data = NULL;

	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_memory_equal(response->data, expected_response.data, sizeof(expected_response_data));

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test verifies that a CCC can be sent */
static void test_send_ccc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	const int CCC_ENEC_DIRECT = 0x80;
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_buffer(bulk_request_id,
								CCC_ENEC_DIRECT,
								&expected_command_buffer,
								DEVICE_ADDRESS,
								USBI3C_WRITE,
								USBI3C_TERMINATE_ON_ANY_ERROR,
								0,
								NULL,
								USBI3C_I3C_SDR_MODE,
								USBI3C_I3C_RATE_2_MHZ,
								USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = NULL;
	expected_response.data_length = 0;
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = CCC_WITHOUT_DEFINING_BYTE;
	command->command_descriptor->common_command_code = CCC_ENEC_DIRECT;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;

	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_null(response->data);

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test verifies that a ccc with defining byte can be sent */
static void test_send_ccc_with_defining_byte(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	const int CCC_RSTACT_BROADCAST = 0x2A;
	const int RESET_PERIPHERAL = 0x01;
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_with_defining_byte_buffer(bulk_request_id,
										   CCC_RSTACT_BROADCAST,
										   RESET_PERIPHERAL,
										   &expected_command_buffer,
										   USBI3C_BROADCAST_ADDRESS,
										   USBI3C_WRITE,
										   USBI3C_TERMINATE_ON_ANY_ERROR,
										   0,
										   NULL,
										   USBI3C_I3C_SDR_MODE,
										   USBI3C_I3C_RATE_2_MHZ,
										   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = NULL;
	expected_response.data_length = 0;
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	command->command_descriptor->common_command_code = CCC_RSTACT_BROADCAST;
	command->command_descriptor->defining_byte = RESET_PERIPHERAL;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->target_address = USBI3C_BROADCAST_ADDRESS;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;

	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_null(response->data);

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test verifies that a ccc with defining byte with a value of 0x00 are accepted and can be sent */
static void test_send_ccc_with_defining_byte_with_value_zero(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	const int CCC_ENEC_BROADCAST = 0x00;
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_ccc_with_defining_byte_buffer(bulk_request_id,
										   CCC_ENEC_BROADCAST,
										   0x00, // the ENEC broadcast CCC doesn't accept a defining byte, but for testing purposes let's ignore that
										   &expected_command_buffer,
										   USBI3C_BROADCAST_ADDRESS,
										   USBI3C_WRITE,
										   USBI3C_TERMINATE_ON_ANY_ERROR,
										   0,
										   NULL,
										   USBI3C_I3C_SDR_MODE,
										   USBI3C_I3C_RATE_2_MHZ,
										   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = NULL;
	expected_response.data_length = 0;
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/*******************/
	/* Enqueue command */
	/*******************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	command->command_descriptor->common_command_code = CCC_ENEC_BROADCAST;
	command->command_descriptor->defining_byte = 0x00;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->target_address = USBI3C_BROADCAST_ADDRESS;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = 0;
	command->data = NULL;

	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_null(response->data);

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test validates that commands can be sent at different I3C mode and rates */
static void test_send_command_at_different_transfer_mode(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response expected_response;
	unsigned char expected_response_data[] = "Response data";
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct list *responses = NULL;
	unsigned char data[] = "Arbitrary test data";
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, USBI3C_I3C_HDR_DDR_MODE, USBI3C_I3C_RATE_6_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

	/* Mocks for sending the bulk request */
	mock_usb_output_bulk_transfer(expected_command_buffer, expected_command_buffer_size, RETURN_SUCCESS);

	/********************************/
	/* Mocks for getting a response */
	/********************************/

	/* let's create the mock response that is going to be "received" from the I3C device */
	expected_response.attempted = USBI3C_COMMAND_ATTEMPTED;
	expected_response.has_data = USBI3C_RESPONSE_HAS_DATA;
	expected_response.error_status = USBI3C_SUCCEEDED;
	expected_response.data = expected_response_data;
	expected_response.data_length = sizeof(expected_response_data);
	expected_response_buffer_size = helper_create_response_buffer(&expected_response_buffer, &expected_response, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

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
	usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, response_cb, &callback_called);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received one response, let's validate it */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, expected_response.attempted);
	assert_int_equal(response->has_data, expected_response.has_data);
	assert_int_equal(response->data_length, expected_response.data_length);
	assert_int_equal(response->error_status, expected_response.error_status);
	assert_memory_equal(response->data, expected_response.data, sizeof(expected_response_data));

	/* only one response should have been sent */
	assert_null(responses->next);

	/* verify that the record was deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* This test validates the following:
 *  - Verifies the function uses the correct endpoint to send data
 *  - Verifies the function creates the correct data buffer that includes all commands and appended data
 *  - Verifies the function the function succeeds on sending the list of commands (using a mock transfer),
 *    and returns the expected value
 */
static void test_send_multiple_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct list *expected_responses = NULL;
	struct usbi3c_response r1, r2, r3;
	unsigned char expected_response_data[] = "Response data!!";
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	unsigned char data1[] = "Arbitrary test data";
	unsigned char data2[] = "Some test data with a length of 35";
	const int BYTES_TO_READ = 16;
	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data1), data1, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_to_command_buffer(bulk_request_id + 1, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL);
	expected_command_buffer_size = helper_add_to_command_buffer(bulk_request_id + 2, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data2), data2);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

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
	r2.has_data = USBI3C_RESPONSE_HAS_DATA;
	r2.error_status = USBI3C_SUCCEEDED;
	r2.data = expected_response_data;
	r2.data_length = sizeof(expected_response_data);
	expected_responses = list_append(expected_responses, &r2);

	r3.attempted = USBI3C_COMMAND_ATTEMPTED;
	r3.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r3.error_status = USBI3C_SUCCEEDED;
	r3.data = NULL;
	r3.data_length = 0;
	expected_responses = list_append(expected_responses, &r3);

	expected_response_buffer_size = helper_create_multiple_response_buffer(&expected_response_buffer, expected_responses, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/********************/
	/* Enqueue commands */
	/********************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* first command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data1);
	command->data = (unsigned char *)calloc(1, sizeof(data1));
	memcpy(command->data, data1, sizeof(data1));
	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* second command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = BYTES_TO_READ;
	command->data = NULL;
	command->on_response_cb = NULL;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* third command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data2);
	command->data = (unsigned char *)calloc(1, sizeof(data2));
	memcpy(command->data, data2, sizeof(data2));
	command->on_response_cb = NULL;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received three responses, let's validate them */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, r1.attempted);
	assert_int_equal(response->has_data, r1.has_data);
	assert_int_equal(response->data_length, r1.data_length);
	assert_int_equal(response->error_status, r1.error_status);
	assert_null(response->data);

	response = (struct usbi3c_response *)responses->next->data;
	assert_int_equal(response->attempted, r2.attempted);
	assert_int_equal(response->has_data, r2.has_data);
	assert_int_equal(response->data_length, r2.data_length);
	assert_int_equal(response->error_status, r2.error_status);
	assert_memory_equal(response->data, r2.data, sizeof(expected_response_data));

	response = (struct usbi3c_response *)responses->next->next->data;
	assert_int_equal(response->attempted, r3.attempted);
	assert_int_equal(response->has_data, r3.has_data);
	assert_int_equal(response->data_length, r3.data_length);
	assert_int_equal(response->error_status, r3.error_status);
	assert_null(response->data);

	/* only three responses should have been sent */
	assert_null(responses->next->next->next);

	/* verify that the records were deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	list_free_list(&expected_responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* Test to validate the command dependency on previous requests gets set correctly */
static void test_send_multiple_dependent_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	/* variables required by the user */
	struct list *responses = NULL;
	struct usbi3c_command *command = NULL;
	unsigned char data1[] = "Arbitrary test data";
	unsigned char data2[] = "Some test data with a length of 35";
	const int BYTES_TO_READ = 16;
	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	int buffer_available = 0;
	struct usbi3c_response r1, r2, r3;
	struct list *expected_responses = NULL;
	unsigned char expected_response_data[] = "Response data!!";
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int callback_called = 0;

	/*******************************/
	/* Mocks for sending a command */
	/*******************************/

	expected_command_buffer_size = helper_create_command_buffer(bulk_request_id, &expected_command_buffer, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data1), data1, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_2_MHZ, USBI3C_DEPENDENT_ON_PREVIOUS);
	expected_command_buffer_size = helper_add_to_command_buffer(bulk_request_id + 1, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL);
	expected_command_buffer_size = helper_add_to_command_buffer(bulk_request_id + 2, &expected_command_buffer, expected_command_buffer_size, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data2), data2);

	/* let's say the buffer available is larger than the required buffer by 100 bytes */
	buffer_available = expected_command_buffer_size + 100;

	/* Mocks for getting the buffer available */
	mock_get_buffer_available(&fake_handle, &buffer_available, RETURN_SUCCESS);

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
	r2.has_data = USBI3C_RESPONSE_HAS_DATA;
	r2.error_status = USBI3C_SUCCEEDED;
	r2.data = expected_response_data;
	r2.data_length = sizeof(expected_response_data);
	expected_responses = list_append(expected_responses, &r2);

	r3.attempted = USBI3C_COMMAND_ATTEMPTED;
	r3.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	r3.error_status = USBI3C_SUCCEEDED;
	r3.data = NULL;
	r3.data_length = 0;
	expected_responses = list_append(expected_responses, &r3);

	expected_response_buffer_size = helper_create_multiple_response_buffer(&expected_response_buffer, expected_responses, bulk_request_id);

	/* add a mock response notification */
	fake_transfer_add_data(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, expected_response_buffer, expected_response_buffer_size);
	mock_libusb_wait_for_events_trigger(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);

	/********************/
	/* Enqueue commands */
	/********************/

	/* NOTE: This test deliberately enqueues the command manually instead of
	 * using usbi3c_enqueue_command() to keep tests independent. */

	/* first command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data1);
	command->data = (unsigned char *)calloc(1, sizeof(data1));
	memcpy(command->data, data1, sizeof(data1));
	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* second command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = BYTES_TO_READ;
	command->data = NULL;
	command->on_response_cb = NULL;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* third command */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->target_address = DEVICE_ADDRESS;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->data_length = sizeof(data2);
	command->data = (unsigned char *)calloc(1, sizeof(data2));
	memcpy(command->data, data2, sizeof(data2));
	command->on_response_cb = NULL;
	command->user_data = NULL;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/* NOTE: we don't need to check for responses for this test, that is already being tested in another test */

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	list_free_list(&expected_responses);
	free(expected_command_buffer);
	free(expected_response_buffer);
}

/* Test to validate the target reset pattern can be sent successfully */
static void test_send_target_reset_pattern(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	/* variables required for the mocks */
	unsigned char *expected_command_buffer = NULL;
	int expected_command_buffer_size = 0;
	struct usbi3c_response r1, r2;
	struct list *expected_responses = NULL;
	unsigned char *expected_response_buffer = NULL;
	int expected_response_buffer_size = 0;
	int buffer_available = 0;
	int request_id = bulk_request_id;

	/* variables required by the user */
	struct usbi3c_command *command = NULL;
	struct list *responses = NULL;
	const int CCC_RSTACT_BROADCAST = 0x2A;
	const int RESET_WHOLE_TARGET = 0x02;

	/* variables required for validation */
	struct usbi3c_response *response = NULL;
	int callback_called = 0;

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
										   0,
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
	command->command_descriptor->data_length = 0;
	command->data = NULL;
	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* second command: the target reset pattern */
	command = (struct usbi3c_command *)calloc(1, sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)calloc(1, sizeof(struct command_descriptor));
	command->command_descriptor->command_type = TARGET_RESET_PATTERN;
	/* let's add a callback to make sure it is NOT used by usbi3c_send_commands() */
	command->on_response_cb = response_cb;
	command->user_data = &callback_called;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/********/
	/* Test */
	/********/

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);

	/***************/
	/* Validations */
	/***************/

	/* we should have received two responses, let's validate them */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, r1.attempted);
	assert_int_equal(response->has_data, r1.has_data);
	assert_int_equal(response->data_length, r1.data_length);
	assert_int_equal(response->error_status, r1.error_status);
	assert_null(response->data);

	response = (struct usbi3c_response *)responses->next->data;
	assert_int_equal(response->attempted, r2.attempted);
	assert_int_equal(response->has_data, r2.has_data);
	assert_int_equal(response->data_length, r2.data_length);
	assert_int_equal(response->error_status, r2.error_status);
	assert_null(response->data);

	/* only two responses should have been sent */
	assert_null(responses->next->next);

	/* verify that the records were deleted from the regular request tracker */
	assert_null(deps->usbi3c_dev->request_tracker->regular_requests->requests);

	/* the callback should not have been called */
	assert_int_equal(callback_called, 0);

	/******************/
	/* Free resources */
	/******************/

	usbi3c_free_responses(&responses);
	list_free_list(&expected_responses);
	free(expected_response_buffer);
	free(expected_command_buffer);
}

int main(void)
{

	/* Unit tests for the usbi3c_send_commands() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_missing_context),
		cmocka_unit_test(test_negative_empty_command_queue),
		cmocka_unit_test(test_negative_missing_command),
		cmocka_unit_test(test_negative_missing_command_descriptor),
		cmocka_unit_test(test_negative_missing_command_data),
		cmocka_unit_test(test_negative_not_enough_buffer_available),
		cmocka_unit_test(test_send_single_write_command),
		cmocka_unit_test(test_send_single_read_command),
		cmocka_unit_test(test_send_command_at_different_transfer_mode),
		cmocka_unit_test(test_send_ccc),
		cmocka_unit_test(test_send_ccc_with_defining_byte),
		cmocka_unit_test(test_send_ccc_with_defining_byte_with_value_zero),
		cmocka_unit_test(test_send_multiple_commands),
		cmocka_unit_test(test_send_multiple_dependent_commands),
		cmocka_unit_test(test_send_target_reset_pattern),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
