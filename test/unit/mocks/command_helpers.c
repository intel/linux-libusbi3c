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

#include <pthread.h>

#include "helpers.h"

static int get_padding(int data_size)
{
	int padding = 0;

	padding = 4 - (data_size % 4);
	padding = padding < 4 ? padding : 0;

	return padding;
}

static int create_transfer_header_buffer(unsigned char *buffer, uint8_t dependent_on_previous)
{
	uint32_t *ptr_32 = NULL;

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)buffer;

	/* 31:3 Reserved, 2 Dependent On Previous, 1:0 Tag (tag is always zero in this case
	 * since the function is for regular commands) */
	*ptr_32 = (dependent_on_previous << 2);

	/* return the size of the buffer (just one double word) */
	return 1 * DWORD_SIZE;
}

static int create_command_block_buffer(unsigned char *buffer, struct usbi3c_command *command, int request_id)
{
	struct command_descriptor *desc = command->command_descriptor;
	uint32_t *ptr_32 = NULL;
	uint8_t *ptr_8 = NULL;
	int padding = 0;
	int buffer_size = 0;
	int has_data = 0;

	if (desc->command_direction != USBI3C_READ && desc->data_length > 0) {
		has_data = USBI3C_RESPONSE_HAS_DATA;
	} else {
		has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	}

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)buffer;

	/* 31:17 Reserved, 16 Has Data, 15:0 Request ID */
	*(ptr_32 += 0) = (has_data << 16) + request_id;
	/* 31:24 TM Specific Info, 23:21 TR, 20:16 TM, 15:8 Address, 7:4 EH, 3 R/W, 2:0 CT */
	*(ptr_32 += 1) = (desc->transfer_rate << 21) + (desc->transfer_mode << 16) + (desc->target_address << 8) + (desc->error_handling << 4) + (desc->command_direction << 3) + desc->command_type;
	/* 31:16 Reserved, 15:8 CCC, 7:0 DB */
	*(ptr_32 += 1) = (desc->common_command_code << 8) + desc->defining_byte;
	/* 31:22 Reserved, 21:0 Data Length */
	*(ptr_32 += 1) = desc->data_length;
	/* 31:0 Reserved */
	*(ptr_32 += 1) = 0x00000000;
	buffer_size += 5 * DWORD_SIZE;

	/* for the data cast the pointer to 8-bit chunks */
	ptr_8 = (uint8_t *)(ptr_32 + 1);

	/* when adding the data to the buffer we need to make sure is 32-bit aligned */
	if (command->data != NULL && desc->data_length > 0) {
		padding = get_padding(desc->data_length);
		memcpy((ptr_8 + padding), command->data, desc->data_length);
		buffer_size += desc->data_length + padding;
	}

	return buffer_size;
}

static int create_command_buffer(int request_id, int command_type, int ccc, int defining_byte, unsigned char **buffer, int target_address, int command_direction,
				 int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous)
{
	struct usbi3c_command *cmd = NULL;
	unsigned char *cmd_buffer = NULL;
	int buffer_size = 0;

	/* create a temporary command struct */
	cmd = bulk_transfer_alloc_command();
	cmd->data = data;
	cmd->command_descriptor->command_type = command_type;
	cmd->command_descriptor->target_address = target_address;
	cmd->command_descriptor->command_direction = command_direction;
	cmd->command_descriptor->error_handling = error_handling;
	cmd->command_descriptor->data_length = data_size;
	cmd->command_descriptor->common_command_code = ccc;
	cmd->command_descriptor->defining_byte = defining_byte;
	cmd->command_descriptor->transfer_mode = transfer_mode;
	cmd->command_descriptor->transfer_rate = transfer_rate;
	cmd->command_descriptor->tm_specific_info = 0;

	/* if the command is a Read then the data_size indicates the number of bytes to
	* read, it doesn't mean this request has data, Read commands have no data, so we
	don't need to allocate memory for it */
	if (command_direction == USBI3C_READ) {
		data_size = 0;
	}

	/* let's allocate memory for a buffer that will contain this command
	 * to get the expected buffer size, we need to add:
	 * - the size of the bulk request transfer header: 1 DW
	 * - the size of 1 bulk request block:  5 DW
	 * - the padded data block of the command */
	buffer_size = (1 * DWORD_SIZE) + (5 * DWORD_SIZE) + data_size + get_padding(data_size);
	*buffer = (unsigned char *)calloc(1, buffer_size);

	/* let's create a copy of the *buffer pointer so we don't loose it */
	cmd_buffer = *buffer;

	/* start assembling the buffer */
	cmd_buffer += create_transfer_header_buffer(cmd_buffer, dependent_on_previous);
	create_command_block_buffer(cmd_buffer, cmd, request_id);

	/* point the cmd->data to NULL first so we don't attempt to free memory
	 * that was allocated somewhere else */
	cmd->data = NULL;
	bulk_transfer_free_command(&cmd);

	return buffer_size;
}

int helper_create_ccc_with_defining_byte_buffer(int request_id, int ccc, int defining_byte, unsigned char **buffer, int target_address, int command_direction,
						int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous)
{
	return create_command_buffer(request_id, CCC_WITH_DEFINING_BYTE, ccc, defining_byte, buffer, target_address, command_direction, error_handling, data_size, data, transfer_mode, transfer_rate, dependent_on_previous);
}

int helper_create_ccc_buffer(int request_id, int ccc, unsigned char **buffer, int target_address, int command_direction,
			     int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous)
{
	return create_command_buffer(request_id, CCC_WITHOUT_DEFINING_BYTE, ccc, 0, buffer, target_address, command_direction, error_handling, data_size, data, transfer_mode, transfer_rate, dependent_on_previous);
}

int helper_create_command_buffer(int request_id, unsigned char **buffer, int target_address, int command_direction,
				 int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous)
{
	return create_command_buffer(request_id, REGULAR_COMMAND, 0, 0, buffer, target_address, command_direction, error_handling, data_size, data, transfer_mode, transfer_rate, dependent_on_previous);
}

static int add_to_command_buffer(int request_id, int command_type, int ccc, int defining_byte, unsigned char **buffer, int current_buffer_size, int target_address,
				 int command_direction, int error_handling, int data_size, unsigned char *data)
{
	struct usbi3c_command *cmd = NULL;
	unsigned char *cmd_buffer = NULL;
	int new_buffer_size = 0;
	int command_buffer_size = 0;

	/* create a temporary command struct */
	cmd = bulk_transfer_alloc_command();
	cmd->data = data;
	cmd->command_descriptor->command_type = command_type;
	cmd->command_descriptor->target_address = target_address;
	cmd->command_descriptor->command_direction = command_direction;
	cmd->command_descriptor->error_handling = error_handling;
	cmd->command_descriptor->data_length = data_size;
	cmd->command_descriptor->common_command_code = ccc;
	cmd->command_descriptor->defining_byte = defining_byte;
	cmd->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	cmd->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	cmd->command_descriptor->tm_specific_info = 0;

	/* if the command is a Read then the data_size indicates the number of bytes to
	* read, it doesn't mean this request has data, Read commands have no data, so we
	don't need to allocate memory for it */
	if (command_direction == USBI3C_READ) {
		data_size = 0;
	}

	/* let's reallocate memory for a buffer that will contain one more command
	 * to get the expected buffer size, we need to add:
	 * - the size of the previous command
	 * - the size of 1 bulk request block:  5 DW
	 * - the padded data block of the command */
	command_buffer_size = (5 * DWORD_SIZE) + data_size + get_padding(data_size);
	new_buffer_size = current_buffer_size + command_buffer_size;
	*buffer = (unsigned char *)realloc(*buffer, new_buffer_size);

	/* let's point to the next section of the buffer */
	cmd_buffer = *buffer + current_buffer_size;

	/* realloc doesn't initialize the memory as calloc does, so let's do it now */
	memset(cmd_buffer, 0, command_buffer_size);

	/* start assembling the buffer at the end of the previous one */
	create_command_block_buffer(cmd_buffer, cmd, request_id);

	/* point the cmd->data to NULL first so we don't attempt to free memory
	 * that was allocated somewhere else */
	cmd->data = NULL;
	bulk_transfer_free_command(&cmd);

	return new_buffer_size;
}

int helper_add_ccc_with_defining_byte_to_command_buffer(int request_id, int ccc, int defining_byte, unsigned char **buffer, int current_buffer_size, int target_address,
							int command_direction, int error_handling, int data_size, unsigned char *data)
{
	return add_to_command_buffer(request_id, CCC_WITH_DEFINING_BYTE, ccc, defining_byte, buffer, current_buffer_size, target_address, command_direction, error_handling, data_size, data);
}

int helper_add_ccc_to_command_buffer(int request_id, int ccc, unsigned char **buffer, int current_buffer_size, int target_address, int command_direction, int error_handling,
				     int data_size, unsigned char *data)
{
	return add_to_command_buffer(request_id, CCC_WITHOUT_DEFINING_BYTE, ccc, 0, buffer, current_buffer_size, target_address, command_direction, error_handling, data_size, data);
}

int helper_add_to_command_buffer(int request_id, unsigned char **buffer, int current_buffer_size, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data)
{
	return add_to_command_buffer(request_id, REGULAR_COMMAND, 0, 0, buffer, current_buffer_size, target_address, command_direction, error_handling, data_size, data);
}

int helper_add_target_reset_pattern_to_command_buffer(int request_id, unsigned char **buffer, int current_buffer_size)
{
	struct usbi3c_command *cmd = NULL;
	unsigned char *cmd_buffer = NULL;
	int new_buffer_size = 0;
	int command_buffer_size = 0;

	/* create a temporary command struct */
	cmd = bulk_transfer_alloc_command();
	cmd->data = NULL;
	cmd->command_descriptor->command_type = TARGET_RESET_PATTERN;
	cmd->command_descriptor->target_address = 0;
	cmd->command_descriptor->command_direction = 0;
	cmd->command_descriptor->error_handling = 0;
	cmd->command_descriptor->data_length = 0;
	cmd->command_descriptor->common_command_code = 0;
	cmd->command_descriptor->defining_byte = 0;
	cmd->command_descriptor->transfer_mode = 0;
	cmd->command_descriptor->transfer_rate = 0;
	cmd->command_descriptor->tm_specific_info = 0;

	/* let's reallocate memory for a buffer that will contain one more command
	 * to get the expected buffer size, we need to add:
	 * - the size of the previous command
	 * - the size of 1 bulk request block:  5 DW */
	command_buffer_size = (5 * DWORD_SIZE);
	new_buffer_size = current_buffer_size + command_buffer_size;
	*buffer = (unsigned char *)realloc(*buffer, new_buffer_size);

	/* let's point to the next section of the buffer */
	cmd_buffer = *buffer + current_buffer_size;

	/* realloc doesn't initialize the memory as calloc does, so let's do it now */
	memset(cmd_buffer, 0, command_buffer_size);

	/* start assembling the buffer at the end of the previous one */
	create_command_block_buffer(cmd_buffer, cmd, request_id);

	bulk_transfer_free_command(&cmd);

	return new_buffer_size;
}

struct usbi3c_command *helper_create_command(on_response_fn on_response_cb, void *user_data, unsigned char **buffer, int *buffer_size, int *request_id)
{
	struct usbi3c_command *command = NULL;
	unsigned char *cmd_buffer = NULL;
	unsigned char data[] = "Some test data with a length of 35";

	/* let's allocate memory for a buffer that will contain these commands
	 * to get the expected buffer size, we need to add:
	 * - the size of the bulk request transfer header: 1 DW
	 * - the size of 1 bulk request block:  5 DW
	 * - the padded data block of the command */
	*buffer_size = (1 * DWORD_SIZE) + (5 * DWORD_SIZE) + sizeof(data) + get_padding(sizeof(data));
	*buffer = (unsigned char *)calloc(1, *buffer_size);

	/* let's create a copy of the *buffer pointer so we don't loose it */
	cmd_buffer = *buffer;

	/* start assembling the buffer */
	cmd_buffer += create_transfer_header_buffer(cmd_buffer, USBI3C_NOT_DEPENDENT_ON_PREVIOUS);

	/* allocate memory for a command */
	command = bulk_transfer_alloc_command();

	/* initialize the command descriptor with an arbitrary test data */
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK;
	command->command_descriptor->target_address = 0x12;
	command->command_descriptor->defining_byte = 0x00;
	command->command_descriptor->common_command_code = 0x00;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->command_descriptor->tm_specific_info = 0;

	/* assign an arbitrary test data to the command */
	command->data = (unsigned char *)calloc(1, sizeof(data));
	memcpy(command->data, data, sizeof(data));
	command->command_descriptor->data_length = sizeof(data);

	/* let's add a callback function */
	command->on_response_cb = on_response_cb;
	command->user_data = user_data;

	*request_id = bulk_request_id;
	create_command_block_buffer(cmd_buffer, command, *request_id);

	return command;
}

struct list *helper_create_commands(on_response_fn on_response_cb, void *user_data, unsigned char **buffer, int *buffer_size, int *request_id, uint8_t dependent_on_previous)
{
	struct list *commands = NULL;
	struct usbi3c_command *command = NULL;
	int req_id;
	unsigned char *cmd_buffer = NULL;
	unsigned char data1[] = "Some arbitrary test data with a length of 51 bytes";
	unsigned char data2[] = "Shorter test data - 29 bytes";

	/* let's return a copy of the current id that are to be used */
	*request_id = bulk_request_id;

	/* let's allocate memory for a buffer that will contain these commands
	 * to get the expected buffer size, we need to add:
	 * - the size of the bulk request transfer header (just one for all commands): 1 DW
	 * - the size of 3 bulk request blocks (one per command):  3 * 5 DW = 15 DW
	 * - the padded data block of each of the two command that contain data */
	*buffer_size = (1 * DWORD_SIZE) + (3 * (5 * DWORD_SIZE)) + sizeof(data1) + get_padding(sizeof(data1)) + sizeof(data2) + get_padding(sizeof(data2));
	*buffer = (unsigned char *)calloc(1, *buffer_size);

	/* let's create a copy of the *buffer pointer so we don't loose it */
	cmd_buffer = *buffer;

	/* start assembling the buffer */
	cmd_buffer += create_transfer_header_buffer(cmd_buffer, dependent_on_previous);

	/*************/
	/* command 1 */
	/*************/
	command = bulk_transfer_alloc_command();

	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->common_command_code = 0;
	command->command_descriptor->defining_byte = 0;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->target_address = 0x01;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->command_descriptor->tm_specific_info = 0;

	command->data = (unsigned char *)calloc(1, sizeof(data1));
	memcpy(command->data, data1, sizeof(data1));
	command->command_descriptor->data_length = sizeof(data1);

	command->on_response_cb = on_response_cb;
	command->user_data = user_data;

	commands = list_append(commands, command);

	req_id = bulk_request_id;
	cmd_buffer += create_command_block_buffer(cmd_buffer, command, req_id);

	/*************/
	/* command 2 */
	/*************/
	command = bulk_transfer_alloc_command();

	command->command_descriptor->command_direction = USBI3C_READ;
	command->command_descriptor->command_type = CCC_WITH_DEFINING_BYTE;
	command->command_descriptor->common_command_code = 0xCC;
	command->command_descriptor->defining_byte = 0xFF;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->target_address = 0x02;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->command_descriptor->tm_specific_info = 0;

	command->data = NULL;
	command->command_descriptor->data_length = 4;

	command->on_response_cb = on_response_cb;
	command->user_data = user_data;

	commands = list_append(commands, command);

	req_id++;
	cmd_buffer += create_command_block_buffer(cmd_buffer, command, req_id);

	/*************/
	/* command 3 */
	/*************/
	command = bulk_transfer_alloc_command();

	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->command_type = CCC_WITHOUT_DEFINING_BYTE;
	command->command_descriptor->common_command_code = 0xCD;
	command->command_descriptor->defining_byte = 0;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->target_address = 0x03;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->command_descriptor->tm_specific_info = 0;

	command->data = (unsigned char *)calloc(1, sizeof(data2));
	memcpy(command->data, data2, sizeof(data2));
	command->command_descriptor->data_length = sizeof(data2);

	command->on_response_cb = on_response_cb;
	command->user_data = user_data;

	commands = list_append(commands, command);

	req_id++;
	cmd_buffer += create_command_block_buffer(cmd_buffer, command, req_id);

	return commands;
}

static int create_response_block_buffer(unsigned char *buffer, struct usbi3c_response *response, int request_id)
{
	uint32_t *ptr_32 = NULL;
	uint8_t *ptr_8 = NULL;
	int padding = 0;
	int_least32_t buffer_size = 0;

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)buffer;

	/* 31:26 Reserved, 25 Attempted, 24 Has Data, 23:16 Reserved, 15:0 Request ID */
	*(ptr_32 += 0) = (response->attempted << 25) + (response->has_data << 24) + request_id;
	/* 31:28 Error Status, 27:22 Reserved, 21:0 Data Length */
	*(ptr_32 += 1) = (response->error_status << 28) + response->data_length;
	/* 31:0 Reserved */
	*(ptr_32 += 1) = 0x00000000;
	buffer_size += (3 * DWORD_SIZE);

	/* for the data cast the pointer to 8-bit chunks */
	ptr_8 = (uint8_t *)(ptr_32 + 1);

	/* the data has to be 32-bit aligned */
	padding = get_padding(response->data_length);
	memcpy((ptr_8 + padding), response->data, response->data_length);
	buffer_size += response->data_length + padding;

	return buffer_size;
}

int helper_create_response_buffer(unsigned char **buffer, struct usbi3c_response *response, int request_id)
{
	unsigned char *cmd_buffer = NULL;
	int buffer_size = 0;
	const int NOT_APPLICABLE = 0;

	/* let's allocate memory for a buffer that will contain the response.
	 * to get the expected buffer size, we need to add:
	 * - the size of the bulk request transfer header: 1 DW
	 * - the size of 1 response block:  3 DW
	 * - the padded data block of the response */
	buffer_size = (1 * DWORD_SIZE) + (3 * DWORD_SIZE) + response->data_length + get_padding(response->data_length);
	*buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	/* let's create a copy of the *buffer pointer so we don't loose it */
	cmd_buffer = *buffer;

	cmd_buffer += create_transfer_header_buffer(cmd_buffer, NOT_APPLICABLE);
	create_response_block_buffer(cmd_buffer, response, request_id);

	return buffer_size;
}

int helper_create_multiple_response_buffer(unsigned char **buffer, struct list *responses, int request_id)
{
	struct list *node = NULL;
	unsigned char *cmd_buffer = NULL;
	int buffer_size = 0;
	const int NOT_APPLICABLE = 0;

	/* let's allocate memory for a buffer that will contain all the responses.
	 * to get the expected buffer size, we need to add:
	 * - the size of the bulk request transfer header (1 for all responses): 1 DW
	 * - the size of the response block (1 for each response):  3 DW
	 * - the size of the padded data block (1 for each response) */
	buffer_size = (1 * DWORD_SIZE);
	for (node = responses; node; node = node->next) {
		struct usbi3c_response *response = (struct usbi3c_response *)node->data;
		buffer_size += (3 * DWORD_SIZE);
		buffer_size += response->data_length;
		buffer_size += get_padding(response->data_length);
	}
	*buffer = (unsigned char *)calloc(1, (size_t)buffer_size);

	/* let's create a copy of the *buffer pointer so we don't loose it */
	cmd_buffer = *buffer;

	cmd_buffer += create_transfer_header_buffer(cmd_buffer, NOT_APPLICABLE);

	for (node = responses; node; node = node->next) {
		struct usbi3c_response *response = (struct usbi3c_response *)node->data;
		cmd_buffer += create_response_block_buffer(cmd_buffer, response, request_id);
		request_id++;
	}

	return buffer_size;
}

void helper_add_request_to_tracker(struct request_tracker *request_tracker, int request_id, int total_commands, struct usbi3c_response *response)
{
	struct regular_request *regular_request = NULL;

	regular_request = (struct regular_request *)malloc_or_die(sizeof(struct regular_request));
	regular_request->request_id = request_id;
	regular_request->total_commands = total_commands;
	regular_request->response = response;
	regular_request->on_response_cb = NULL;
	pthread_mutex_lock(request_tracker->regular_requests->mutex);
	request_tracker->regular_requests->requests = list_append(request_tracker->regular_requests->requests, regular_request);
	pthread_mutex_unlock(request_tracker->regular_requests->mutex);
}
