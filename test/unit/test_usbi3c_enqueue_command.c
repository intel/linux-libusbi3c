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

const int DEVICE_ADDRESS = 5;

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);

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

static int test_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	bulk_transfer_free_commands(&deps->usbi3c_dev->command_queue);

	return 0;
}

int response_cb(struct usbi3c_response *response, void *user_data)
{
	return response->error_status;
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_usbi3c_enqueue_command_missing_context(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_enqueue_command(NULL, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_usbi3c_enqueue_ccc_missing_context(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int ENTDAA_CCC = 0x07;

	ret = usbi3c_enqueue_ccc(NULL, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, ENTDAA_CCC, 0, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_usbi3c_enqueue_ccc_with_defining_byte_missing_context(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int MLANE_CCC = 0x2D;

	ret = usbi3c_enqueue_ccc_with_defining_byte(NULL, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, MLANE_CCC, 0x00, 0, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the function handles a missing I3C mode gracefully */
static void test_negative_missing_i3c_mode(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char data[] = "Test data";
	int ret = 0;

	/* force a failure by removing the i3c mode from the context */
	struct i3c_mode *tmp = deps->usbi3c_dev->i3c_mode;
	deps->usbi3c_dev->i3c_mode = NULL;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);

	deps->usbi3c_dev->i3c_mode = tmp;
}

/* Negative test to verify that a usbi3c Write command with missing data cannot be added to the command queue */
static void test_negative_missing_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	const int ARBITRARY_SIZE = 10;
	int ret = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, ARBITRARY_SIZE, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify that a usbi3c command with invalid data size cannot be added to the command queue */
static void test_negative_bad_data_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char data[] = "Test data";
	int ret = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0, data, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify that a usbi3c CCC with defining byte that is missing the CCC cannot be added to the command queue */
static void test_negative_missing_ccc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0, 0x10, 0, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify a usbi3c Read command cannot have data */
static void test_negative_read_command_with_data(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char data[] = "Test data";
	int ret = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, sizeof(data), data, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify a usbi3c Read command requires the data size to be read */
static void test_negative_read_command_with_no_data_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 0, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the data size to read with a usbi3c Read command has to be 32-bit aligned */
static void test_negative_read_command_with_unaligned_data_size(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int BYTES_TO_READ = 6; // not 32-bit aligned

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the a callback has to be provided if user data is provided */
static void test_negative_command_with_user_data_and_no_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;
	const int BYTES_TO_READ = 8;
	int user_data = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL, NULL, &user_data);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify the function handles a missing context gracefully */
static void test_negative_missing_context_in_target_reset_pattern(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	ret = usbi3c_enqueue_target_reset_pattern(NULL, NULL, NULL);
	assert_int_equal(ret, -1);
	assert_null(deps->usbi3c_dev->command_queue);
}

/* Negative test to verify that the functions handles incompatible commands in the queue gracefully */
static void test_negative_target_reset_pattern_with_incompatible_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int ret = 0;
	const int BROADCAST_RSTACT = 0x2A;

	/* we need to add an incompatible command to the queue first */
	command = (struct usbi3c_command *)malloc_or_die(sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)malloc_or_die(sizeof(struct command_descriptor));
	command->command_descriptor->command_type = REGULAR_COMMAND;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, -1);

	/* let's modify the command in the queue to be a RSTACT CCC but without error handling */
	((struct usbi3c_command *)deps->usbi3c_dev->command_queue->data)->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	((struct usbi3c_command *)deps->usbi3c_dev->command_queue->data)->command_descriptor->common_command_code = BROADCAST_RSTACT;
	((struct usbi3c_command *)deps->usbi3c_dev->command_queue->data)->command_descriptor->error_handling = USBI3C_DO_NOT_TERMINATE_ON_ERROR_INCLUDING_NACK;

	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, -1);
}

/* Verify that a usbi3c command can be added to the command queue */
static void test_enqueue_command(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	unsigned char data1[] = "Arbitrary test data";
	unsigned char data2[] = "More data for the second command";
	int data1_size = sizeof(data1);
	int data2_size = sizeof(data2);
	int ret = -1;
	const int BYTES_TO_READ = 4;
	int user_data = 0;

	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data1_size, data1, NULL, NULL);
	assert_int_equal(ret, 0);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, data2_size, data2, response_cb, &user_data);
	assert_int_equal(ret, 0);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, BYTES_TO_READ, NULL, NULL, NULL);
	assert_int_equal(ret, 0);

	/* verify the commands were added to the queue correctly */
	assert_non_null(deps->usbi3c_dev->command_queue);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->data;
	assert_non_null(command);
	assert_memory_equal(command->data, data1, data1_size);
	assert_null(command->on_response_cb);
	assert_int_equal(command->command_descriptor->target_address, DEVICE_ADDRESS);
	assert_int_equal(command->command_descriptor->command_direction, USBI3C_WRITE);
	assert_int_equal(command->command_descriptor->error_handling, USBI3C_TERMINATE_ON_ANY_ERROR);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->next->data;
	assert_non_null(command);
	assert_memory_equal(command->data, data2, data2_size);
	assert_ptr_equal(command->on_response_cb, response_cb);
	assert_int_equal(command->command_descriptor->target_address, DEVICE_ADDRESS);
	assert_int_equal(command->command_descriptor->command_direction, USBI3C_WRITE);
	assert_int_equal(command->command_descriptor->error_handling, USBI3C_TERMINATE_ON_ANY_ERROR);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->next->next->data;
	assert_non_null(command);
	assert_null(command->data);
	assert_null(command->on_response_cb);
	assert_int_equal(command->command_descriptor->target_address, DEVICE_ADDRESS);
	assert_int_equal(command->command_descriptor->command_direction, USBI3C_READ);
	assert_int_equal(command->command_descriptor->error_handling, USBI3C_TERMINATE_ON_ANY_ERROR);
}

/* Verify that a usbi3c CCC can be added to the command queue */
static void test_enqueue_ccc(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	unsigned char data1[] = "Arbitrary test data";
	int data1_size = sizeof(data1);
	int ret = -1;
	const int BYTES_TO_READ = 4;

	ret = usbi3c_enqueue_ccc_with_defining_byte(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0x11, 0x21, data1_size, data1, NULL, NULL);
	assert_int_equal(ret, 0);
	ret = usbi3c_enqueue_command(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_WRITE, USBI3C_TERMINATE_ON_ANY_ERROR, 0, NULL, response_cb, NULL);
	assert_int_equal(ret, 0);
	ret = usbi3c_enqueue_ccc(deps->usbi3c_dev, DEVICE_ADDRESS, USBI3C_READ, USBI3C_TERMINATE_ON_ANY_ERROR, 0x22, BYTES_TO_READ, NULL, NULL, NULL);
	assert_int_equal(ret, 0);

	/* verify the commands were added to the queue correctly */
	assert_non_null(deps->usbi3c_dev->command_queue);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->data;
	assert_non_null(command);
	assert_int_equal(command->command_descriptor->common_command_code, 0x11);
	assert_int_equal(command->command_descriptor->defining_byte, 0x21);
	assert_int_equal(command->command_descriptor->command_type, CCC_WITH_DEFINING_BYTE);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->next->data;
	assert_non_null(command);
	assert_int_equal(command->command_descriptor->common_command_code, 0);
	assert_int_equal(command->command_descriptor->defining_byte, 0);
	assert_int_equal(command->command_descriptor->command_type, REGULAR_COMMAND);

	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->next->next->data;
	assert_non_null(command);
	assert_int_equal(command->command_descriptor->common_command_code, 0x22);
	assert_int_equal(command->command_descriptor->defining_byte, 0);
	assert_int_equal(command->command_descriptor->command_type, CCC_WITHOUT_DEFINING_BYTE);
}

/* Verify that a target reset pattern can be added to an empty command queue */
static void test_enqueue_target_reset_pattern_empty_queue(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int ret = -1;

	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, NULL, NULL);
	assert_int_equal(ret, 0);

	/* verify the reset pattern was added to the queue */
	assert_non_null(deps->usbi3c_dev->command_queue);
	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->data;
	assert_non_null(command);
	assert_int_equal(command->command_descriptor->command_type, TARGET_RESET_PATTERN);

	/* everything else in the command descriptor should be set to zero */
	assert_int_equal(command->command_descriptor->command_direction, 0);
	assert_int_equal(command->command_descriptor->common_command_code, 0);
	assert_int_equal(command->command_descriptor->data_length, 0);
	assert_int_equal(command->command_descriptor->defining_byte, 0);
	assert_int_equal(command->command_descriptor->error_handling, 0);
	assert_int_equal(command->command_descriptor->target_address, 0);
	assert_int_equal(command->command_descriptor->tm_specific_info, 0);
	assert_int_equal(command->command_descriptor->transfer_mode, 0);
	assert_int_equal(command->command_descriptor->transfer_rate, 0);
	assert_null(command->data);
	assert_null(command->on_response_cb);
}

/* Verify that a target reset pattern can be added to the command queue where there are only compatible commands in it */
static void test_enqueue_target_reset_pattern_compatible_command_in_queue(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct usbi3c_command *command = NULL;
	int ret = -1;
	const int BROADCAST_RSTACT = 0x2A;
	int flag = 0;

	/* let's add another target reset pattern to the queue */
	command = (struct usbi3c_command *)malloc_or_die(sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)malloc_or_die(sizeof(struct command_descriptor));
	command->command_descriptor->command_type = TARGET_RESET_PATTERN;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	/* and let's add one more with a RSTACT CCC with defining byte and error handling */
	command = (struct usbi3c_command *)malloc_or_die(sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)malloc_or_die(sizeof(struct command_descriptor));
	command->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	command->command_descriptor->common_command_code = BROADCAST_RSTACT;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	deps->usbi3c_dev->command_queue = list_append(deps->usbi3c_dev->command_queue, command);

	ret = usbi3c_enqueue_target_reset_pattern(deps->usbi3c_dev, response_cb, &flag);
	assert_int_equal(ret, 0);

	/* verify the reset pattern was added to the queue */
	assert_non_null(deps->usbi3c_dev->command_queue);
	assert_non_null(deps->usbi3c_dev->command_queue->next);
	assert_non_null(deps->usbi3c_dev->command_queue->next->next);
	command = (struct usbi3c_command *)deps->usbi3c_dev->command_queue->next->next->data;
	assert_non_null(command);
	assert_int_equal(command->command_descriptor->command_type, TARGET_RESET_PATTERN);
}

int main(void)
{

	/* Unit tests for the test_usbi3c_enqueue_command() function */
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_negative_usbi3c_enqueue_command_missing_context, test_teardown),
		cmocka_unit_test_teardown(test_negative_usbi3c_enqueue_ccc_missing_context, test_teardown),
		cmocka_unit_test_teardown(test_negative_usbi3c_enqueue_ccc_with_defining_byte_missing_context, test_teardown),
		cmocka_unit_test_teardown(test_negative_missing_i3c_mode, test_teardown),
		cmocka_unit_test_teardown(test_negative_missing_data, test_teardown),
		cmocka_unit_test_teardown(test_negative_bad_data_size, test_teardown),
		cmocka_unit_test_teardown(test_negative_missing_ccc, test_teardown),
		cmocka_unit_test_teardown(test_negative_read_command_with_no_data_size, test_teardown),
		cmocka_unit_test_teardown(test_negative_read_command_with_data, test_teardown),
		cmocka_unit_test_teardown(test_negative_read_command_with_unaligned_data_size, test_teardown),
		cmocka_unit_test_teardown(test_negative_command_with_user_data_and_no_callback, test_teardown),
		cmocka_unit_test_teardown(test_negative_missing_context_in_target_reset_pattern, test_teardown),
		cmocka_unit_test_teardown(test_negative_target_reset_pattern_with_incompatible_command, test_teardown),
		cmocka_unit_test_teardown(test_enqueue_command, test_teardown),
		cmocka_unit_test_teardown(test_enqueue_ccc, test_teardown),
		cmocka_unit_test_teardown(test_enqueue_target_reset_pattern_empty_queue, test_teardown),
		cmocka_unit_test_teardown(test_enqueue_target_reset_pattern_compatible_command_in_queue, test_teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
