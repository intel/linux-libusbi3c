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

struct callback_data {
	int counter;
	struct usbi3c_response *response;
};

int response_cb(struct usbi3c_response *response, void *user_data)
{
	struct callback_data *cb_data = (struct callback_data *)user_data;

	cb_data->counter++;
	memcpy(cb_data->response, response, sizeof(struct usbi3c_response));
	if (response->data_length > 0) {
		cb_data->response->data = (unsigned char *)calloc(response->data_length, sizeof(unsigned char));
		memcpy(cb_data->response->data, response->data, response->data_length);
	}

	return 0;
}

static void free_cb_data(struct callback_data *cb_data)
{
	if (cb_data == NULL) {
		return;
	}
	if (cb_data->response && cb_data->response->data) {
		free(cb_data->response->data);
	}
	if (cb_data->response) {
		free(cb_data->response);
	}
	free(cb_data);
}

/* Test to verify that we can submit a write command to an I3C device */
static void test_submit_write_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	unsigned char data[] = "Arbitrary test data";
	int data_size = sizeof(data);
	int ret = -1;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* add the command to the queue */
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data_size, data, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten a response through the callback */
	assert_int_equal(cb_data->counter, 1);
	assert_non_null(cb_data->response);
	assert_int_equal(cb_data->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(cb_data->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(cb_data->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(cb_data->response->data_length, 0);

	free_cb_data(cb_data);
}

/* Test to verify that we can submit a read command to an I3C device */
static void test_submit_read_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	int ret = -1;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* add the command to the queue */
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 20, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten a response through the callback */
	assert_int_equal(cb_data->counter, 1);
	assert_non_null(cb_data->response);
	assert_int_equal(cb_data->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(cb_data->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(cb_data->response->has_data, USBI3C_RESPONSE_HAS_DATA);
	assert_true(cb_data->response->data_length > 0);
	assert_non_null(cb_data->response->data);
	assert_memory_equal(cb_data->response->data, "Arbitrary test data", cb_data->response->data_length);

	free_cb_data(cb_data);
}

/* Test to verify that we can submit a CCC to an I3C device */
static void test_submit_ccc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	const int RSTDAA = 0x06;
	int ret = 0;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* add the command to the queue */
	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK, RSTDAA, 0, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten a response through the callback */
	assert_int_equal(cb_data->counter, 1);
	assert_non_null(cb_data->response);
	assert_int_equal(cb_data->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(cb_data->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(cb_data->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(cb_data->response->data_length, 0);
	assert_null(cb_data->response->data);

	free_cb_data(cb_data);
}

/* Test to verify that we can submit a CCC with defining byte to an I3C device */
static void test_submit_ccc_with_defining_byte(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	const int MLANE = 0x2D;	   // CCC
	const int RESET_ML = 0x7F; // Defining byte
	int ret = 0;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* add the command to the queue */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK, MLANE, RESET_ML, 0, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten a response through the callback */
	assert_int_equal(cb_data->counter, 1);
	assert_non_null(cb_data->response);
	assert_int_equal(cb_data->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(cb_data->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(cb_data->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(cb_data->response->data_length, 0);
	assert_null(cb_data->response->data);

	free_cb_data(cb_data);
}

/* Test to verify that we can submit  multiple commands and CCCs in the same transfer to an I3C device */
static void test_submit_multiple_commands(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	const int MLANE = 0x2D;	   // CCC
	const int RESET_ML = 0x7F; // Defining byte
	const int RSTDAA = 0x06;   // CCC
	unsigned char data[] = "Arbitrary test data";
	int data1_size = sizeof(data);
	int ret = 0;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));
	;

	/* enqueue the commands/CCCs to be sent */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK, MLANE, RESET_ML, 0, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data1_size, data, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, 0, RSTDAA, 0, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 20, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* the callback should have gotten called 4 times, once per command */
	assert_int_equal(cb_data->counter, 4);

	/* we should have gotten the data from the READ command through the callback */
	assert_non_null(cb_data->response->data);
	assert_memory_equal(cb_data->response->data, data, sizeof(data));

	free_cb_data(cb_data);
}

/* Test to verify that we can submit a reset to an I3C device */
static void test_submit_reset(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	int ret = 0;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* and we enqueue a target reset pattern to trigger the reset */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* we should have gotten a response through the callback */
	assert_int_equal(cb_data->counter, 1);
	assert_non_null(cb_data->response);
	assert_int_equal(cb_data->response->attempted, USBI3C_COMMAND_ATTEMPTED);
	assert_int_equal(cb_data->response->error_status, USBI3C_SUCCEEDED);
	assert_int_equal(cb_data->response->has_data, USBI3C_RESPONSE_HAS_NO_DATA);
	assert_int_equal(cb_data->response->data_length, 0);
	assert_null(cb_data->response->data);

	free_cb_data(cb_data);
}

/* Test to verify that we can submit a reset action to an I3C device */
static void test_submit_reset_action(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct callback_data *cb_data = NULL;
	const int RSTACT = 0x2A;
	const int RESET_I3C_PERIPHERAL = 0x01;
	int ret = 0;

	cb_data = (struct callback_data *)calloc(1, sizeof(struct callback_data));
	cb_data->counter = 0;
	cb_data->response = (struct usbi3c_response *)calloc(1, sizeof(struct usbi3c_response));

	/* first we need to configure a broadcast reset action */
	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, USBI3C_BROADCAST_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, RSTACT, RESET_I3C_PERIPHERAL, 0, NULL, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* and we enqueue a target reset pattern to trigger the reset */
	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, response_cb, cb_data);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	ret = usbi3c_submit_commands(deps->usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS);
	assert_int_equal(ret, 0);
	assert_int_equal(usbi3c_get_usb_error(deps->usbi3c_dev), RETURN_SUCCESS);

	/* the callback should have been called twice */
	assert_int_equal(cb_data->counter, 2);

	free_cb_data(cb_data);
}

int main(void)
{

	/* Tests for submitting commands */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_submit_write_command, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_read_command, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_ccc, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_ccc_with_defining_byte, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_multiple_commands, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_reset, test_setup, test_teardown),
		cmocka_unit_test_setup_teardown(test_submit_reset_action, test_setup, test_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
