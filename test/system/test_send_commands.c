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

#include "helpers/test_helpers.h"

const int DEVICE_ADDRESS = 5;
const int TIMEOUT = 10;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int test_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_controller_init();
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	*state = deps;

	return 0;
}

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	usbi3c_device_deinit(&deps->usbi3c_dev);
	free(deps);

	return 0;
}

/* Test to verify that we can send a write command to an I3C device */
static void test_send_write_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	unsigned char data[] = "Arbitrary test data";
	int data_size = sizeof(data);
	int ret = 0;

	/* enqueue the command to be sent */
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data_size, data, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* send the command and wait for its response */
	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we sent only one command, we should have gotten only one response */
	assert_int_equal(list_len(responses), 1);

	/* verify that the response we got is correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(response->data_length, 0);

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send a read command to an I3C device */
static void test_send_read_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	int ret = 0;

	/* add the command to the queue */
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 20, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten only one response */
	assert_int_equal(list_len(responses), 1);

	/* verify that the response we got is correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_true(response->data_length > 0);
	assert_non_null(response->data);
	assert_memory_equal(response->data, "Arbitrary test data", response->data_length);

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send a CCC to an I3C device */
static void test_send_ccc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	const int RSTDAA = 0x06;
	int ret = 0;

	/* add the command to the queue */
	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK, RSTDAA, 0, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten only one response */
	assert_int_equal(list_len(responses), 1);

	/* verify that the response we got is correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(response->data_length, 0);
	assert_null(response->data);

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send a CCC with the defining byte to an I3C device */
static void test_send_ccc_with_defining_byte(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	const int MLANE = 0x2D;	   // CCC
	const int RESET_ML = 0x7F; // Defining byte
	int ret = 0;

	/* add the command to the queue */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_DO_NOT_TERMINATE_ON_ERROR_INCLUDING_NACK, MLANE, RESET_ML, 0, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten only one response */
	assert_int_equal(list_len(responses), 1);

	/* verify that the response we got is correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(response->data_length, 0);
	assert_null(response->data);

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send multiple commands and CCCs in the same transfer to an I3C device */
static void test_send_multiple_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct list *node = NULL;
	struct usbi3c_response *response = NULL;
	const int MLANE = 0x2D;	   // CCC
	const int RESET_ML = 0x7F; // Defining byte
	const int RSTDAA = 0x06;   // CCC
	unsigned char data1[] = "Arbitrary test data";
	int data1_size = sizeof(data1);
	int ret = 0;

	/* enqueue the commands/CCCs to be sent */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, 0, MLANE, RESET_ML, 0, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data1_size, data1, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, 0, RSTDAA, 0, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 20, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten one response per command */
	assert_int_equal(list_len(responses), 4);

	/* verify that the response we got is correct */
	for (node = responses; node; node = node->next) {
		response = (struct usbi3c_response *)node->data;
		assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
		assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	}

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send a reset to an I3C device */
static void test_send_reset(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	int ret = 0;

	/* and we enqueue a target reset pattern to trigger the reset */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten two responses */
	assert_int_equal(list_len(responses), 1);

	/* verify that the responses we got are correct */
	response = (struct usbi3c_response *)responses->data;
	assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(response->data_length, 0);
	assert_null(response->data);

	usbi3c_free_responses(&responses);
}

/* Test to verify that we can send a reset action to an I3C device */
static void test_send_reset_action(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct list *responses = NULL;
	struct list *node = NULL;
	struct usbi3c_response *response = NULL;
	const int RSTACT = 0x2A;
	const int RESET_I3C_PERIPHERAL = 0x01;
	int ret = 0;

	/* first we need to configure a broadcast reset action */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, RSTACT, RESET_I3C_PERIPHERAL, 0, NULL, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* and we enqueue a target reset pattern to trigger the reset */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	responses = usbi3c_send_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	assert_non_null(responses);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten two responses */
	assert_int_equal(list_len(responses), 2);

	/* verify that the responses we got are correct */
	for (node = responses; node; node = node->next) {
		response = (struct usbi3c_response *)node->data;
		assert_int_equal(response->attempted, USBI3C_COMMAND_ATTEMPTED);
		assert_int_equal(response->error_status, USBI3C_SUCCEEDED);
		assert_int_equal(response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
		assert_int_equal(response->data_length, 0);
		assert_null(response->data);
	}

	usbi3c_free_responses(&responses);
}

int main(void)
{

	/* Tests for sending commands */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_send_write_command, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_read_command, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_ccc, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_ccc_with_defining_byte, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_multiple_commands, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_reset_action, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_send_reset, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
