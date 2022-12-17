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
#include <string.h>
#include <time.h>

#include "usbi3c_i.h"

#include "ibi_i.h"
#include "ibi_response_i.h"

/* global variable that holds a monotonically increasing ID. */
uint16_t bulk_request_id = 0;

/**
 * @brief Struct that contains the context required to cancel a stalled request.
 */
struct cancel_stall_request_context {
	struct bulk_requests *regular_requests; ///< The regular requests tracker
	uint16_t request_id;			///< The request ID of the command that the I3C controller stalled on
};

/* Generates a monotonically increasing ID */
static uint16_t get_request_id(void)
{
	uint16_t id = bulk_request_id;
	bulk_request_id = (bulk_request_id + 1) % UINT16_MAX;

	return id;
}

/**
 * @brief Frees the memory allocated for a regular request data structure.
 *
 * @param[in] regular_request the request data structure to free
 */
static void bulk_transfer_free_regular_request(struct regular_request **request)
{
	if (*request == NULL) {
		return;
	}
	if ((*request)->response) {
		bulk_transfer_free_response(&(*request)->response);
	}
	FREE(*request);
}

/**
 * @brief Wrapper to bulk_transfer_free_regular_request() to be used to free a list of requests.
 *
 * @param[in] data the regular_request struct to free
 */
static void free_regular_request_in_list(void *data)
{
	struct regular_request *request = (struct regular_request *)data;

	bulk_transfer_free_regular_request(&request);
}

/**
 * @brief Frees the memory allocated for a usbi3c command.
 *
 * This includes the command descriptor and the data, if any.
 *
 * @param[in] command the command pointer pointing to the memory space to be freed
 */
void bulk_transfer_free_command(struct usbi3c_command **command)
{
	if (*command == NULL) {
		return;
	}

	if ((*command)->command_descriptor) {
		FREE((*command)->command_descriptor);
	}
	if ((*command)->data) {
		FREE((*command)->data);
	}
	FREE(*command);
}

/*
 * @brief Wrapper to bulk_transfer_free_command() to be used to free lists of commands.
 *
 * The list_free_list_and_data() function receives as one of its arguments a  function of
 * the type list_free_data_fn_t, and this has a void pointer as argument. This wrapper
 * function type casts the data pointed by the void pointer to the correct type so it can
 * be used to free a list of commands.
 *
 * @param[in] data the usbi3c_command struct to free.
 *
 * @note This function should be used as an argument of function list_free_list_and_data().
 */
static void free_command_in_list(void *data)
{
	struct usbi3c_command *command = (struct usbi3c_command *)data;

	bulk_transfer_free_command(&command);
}

/**
 * @brief Frees the memory allocated for a list of usbi3c commands.
 *
 * @param[in] commands the list of commands (struct usbi3c_command) to be freed.
 */
void bulk_transfer_free_commands(struct list **commands)
{
	list_free_list_and_data(commands, free_command_in_list);
}

/**
 * @brief Frees the memory allocated for a usbi3c response.
 *
 * This includes the data, if any.
 *
 * @param[in] response the response pointer pointing to the memory space to be freed
 */
void bulk_transfer_free_response(struct usbi3c_response **response)
{
	if (*response == NULL) {
		return;
	}

	if ((*response)->data) {
		FREE((*response)->data);
	}
	FREE(*response);
}

/**
 * @brief Initializes the I3C communication mode and rate.
 *
 * It is important to note that the I3C Bus is always initialized and configured in SDR Mode,
 * never in any of the HDR Modes. SDR Mode is the default Mode of the I3C Bus. SDR Mode is
 * also used to enter other Modes, sub-Modes, and states; and for built-in features such as
 * Common Commands (CCCs), In-Band Interrupts, and transition from I2C to I3C by assignment
 * of a Dynamic Address.
 *
 * @return the initialized I3C mode
 */
struct i3c_mode *i3c_mode_init(void)
{
	struct i3c_mode *i3c_mode = NULL;

	i3c_mode = (struct i3c_mode *)malloc_or_die(sizeof(struct i3c_mode));

	/* Set I3C communication mode default values */
	i3c_mode->transfer_mode = DEFAULT_TRANSFER_MODE;
	i3c_mode->transfer_rate = DEFAULT_TRANSFER_RATE;
	i3c_mode->tm_specific_info = 0;

	return i3c_mode;
}

/**
 * @brief Handles the "Stall on Nack" notification
 *
 * This function is called when a stall on nack notification is received from the
 * I3C function.
 *
 * @param[in] notification the notification structure for "stall on nack", which contains the request_tracker
 * @param[in] user_data a void pointer containing the request tracker
 */
void stall_on_nack_handle(struct notification *notification, void *user_data)
{
	struct request_tracker *request_tracker = NULL;
	struct regular_request *request = NULL;
	int ret = -1;

	/* search for the stalled request */
	request_tracker = (struct request_tracker *)user_data;
	pthread_mutex_lock(request_tracker->regular_requests->mutex);
	request = (struct regular_request *)list_search(request_tracker->regular_requests->requests, &notification->code, compare_request_id);
	pthread_mutex_unlock(request_tracker->regular_requests->mutex);
	if (request == NULL) {
		DEBUG_PRINT("The request with id %d referred to in the 'Stall on Nack' notification was not found in the request tracker\n",
			    notification->code);
		return;
	}

	/* Reattempt the request as long as it is under the max number of attempts specified,
	 * if not just cancel the request */
	if (request->reattempt_count < request_tracker->reattempt_max) {
		ret = bulk_transfer_resume_request_async(request_tracker->usb_dev);
		pthread_mutex_lock(request_tracker->regular_requests->mutex);
		request->reattempt_count++;
		pthread_mutex_unlock(request_tracker->regular_requests->mutex);
	} else {
		ret = bulk_transfer_cancel_request_async(request_tracker->usb_dev, request_tracker->regular_requests, notification->code);
	}

	if (ret < 0) {
		DEBUG_PRINT("There was a problem resuming/cancelling the stalled request with ID: %d\n", notification->code);
	}
}

/**
 * @brief Clean up function for the request tracker.
 *
 * @param[in] request_tracker the request tracker structure
 */
void request_tracker_destroy(struct request_tracker **request_tracker)
{
	if (request_tracker == NULL || *request_tracker == NULL) {
		return;
	}
	/* free regular request tracker */
	pthread_mutex_lock((*request_tracker)->regular_requests->mutex);
	list_free_list_and_data(&(*request_tracker)->regular_requests->requests, free_regular_request_in_list);
	pthread_mutex_unlock((*request_tracker)->regular_requests->mutex);
	pthread_mutex_destroy((*request_tracker)->regular_requests->mutex);
	FREE((*request_tracker)->regular_requests->mutex);
	FREE((*request_tracker)->regular_requests);

	FREE((*request_tracker)->vendor_request);
	FREE(*request_tracker);
}

/**
 * @brief Initializes the request transfer tracker.
 *
 * This initializes the request tracker along with the regular request tracker and the vendor
 * specific request tracker.
 *
 * @param[in] usb_dev the representation of the USB device
 * @param[in] ibi_response_queue the queue to store the IBI responses
 * @param[in] ibi IBI structure to store the IBI information
 * @return the initialized request tracker
 */
struct request_tracker *bulk_transfer_request_tracker_init(struct usb_device *usb_dev, struct ibi_response_queue *ibi_response_queue, struct ibi *ibi)
{
	struct request_tracker *request_tracker = NULL;
	const int DEFAULT_REATTEMPT_MAX_FOR_STALLED_REQUESTS = 2;

	request_tracker = (struct request_tracker *)malloc_or_die(sizeof(struct request_tracker));
	request_tracker->reattempt_max = DEFAULT_REATTEMPT_MAX_FOR_STALLED_REQUESTS;
	request_tracker->usb_dev = usb_dev;
	request_tracker->ibi = ibi;
	request_tracker->ibi_response_queue = ibi_response_queue;

	/* initialize the vendor request handler */
	request_tracker->vendor_request = (struct vendor_specific_request *)malloc_or_die(sizeof(struct vendor_specific_request));
	request_tracker->vendor_request->on_vendor_response_cb = NULL;
	request_tracker->vendor_request->user_data = NULL;

	/* initialize the regular request tracker */
	request_tracker->regular_requests = (struct bulk_requests *)malloc_or_die(sizeof(struct bulk_requests));
	request_tracker->regular_requests->requests = NULL;
	request_tracker->regular_requests->mutex = (pthread_mutex_t *)malloc_or_die(sizeof(pthread_mutex_t));
	pthread_mutex_init(request_tracker->regular_requests->mutex, NULL);

	return request_tracker;
}

/**
 * @brief Compares the request ids provided.
 *
 * @param[in] a a bulk request to have its request id compared to
 * @param[in] b the request id we are comparing against
 * @return 0 if the ids are equal
 * @return -1 if a is lower than b
 * @return 1 if a is bigger than b
 */
int compare_request_id(const void *a, const void *b)
{
	if (((struct regular_request *)a)->request_id < *(uint16_t *)b) {
		return -1;
	} else if (((struct regular_request *)a)->request_id > *(uint16_t *)b) {
		return 1;
	}
	/* equal */
	return 0;
}

/**
 * @brief Requests the size of the buffer available for bulk requests
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[out] buffer_available the size in bytes of the buffer available in the I3C function
 * @return 0 if the size of buffer was obtained correctly, or -1 otherwise
 */
static int bulk_transfer_get_buffer_available(struct usbi3c_device *usbi3c_dev, uint32_t *buffer_available)
{
	uint8_t *buffer = NULL;

	/* the max buffer size we are expecting is an unsigned integer
	 * that can fit in 4 bytes */
	buffer = (uint8_t *)malloc_or_die(DWORD_SIZE);

	if (usb_input_control_transfer(usbi3c_dev->usb_dev,
				       GET_BUFFER_AVAILABLE,
				       0,
				       USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
				       buffer,
				       DWORD_SIZE) < 0) {
		FREE(buffer);
		return -1;
	}
	memcpy(buffer_available, buffer, DWORD_SIZE);

	FREE(buffer);

	return 0;
}

/**
 * @brief Validates a command for compliance.
 *
 * @param[in] command the usbi3c command to be validated
 * @return 0 if the command is valid, or -1 otherwise.
 */
int bulk_transfer_validate_command(struct usbi3c_command *command)
{
	struct command_descriptor *command_desc = NULL;

	if (command == NULL) {
		DEBUG_PRINT("A command is missing, aborting...\n");
		return -1;
	}
	if (command->command_descriptor == NULL) {
		DEBUG_PRINT("A command descriptor is missing, aborting...\n");
		return -1;
	}
	command_desc = command->command_descriptor;

	/* TODO: evaluate if it is a good idea to add more validations
	 * in the future, for example, verify the selected values are
	 * within the correct range (i.e. direction can only be 0 (USBI3C_WRITE)
	 * or 1 (USBI3C_READ). */

	/* let's check the command descriptor conditions */
	if (command_desc->command_direction == USBI3C_READ && command_desc->data_length == 0) {
		DEBUG_PRINT("The 'Read' command requires a data size to specify the number of bytes to read, aborting...\n");
		return -1;
	}
	if (command_desc->command_direction == USBI3C_READ && command->data != NULL) {
		DEBUG_PRINT("The 'Read' command cannot have data, aborting...\n");
		return -1;
	}
	if (command_desc->command_direction != USBI3C_READ && (command_desc->data_length > 0 && command->data == NULL)) {
		DEBUG_PRINT("Required data for a command is missing, aborting...\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Creates a buffer with data from a command to be transferred via bulk transfer.
 *
 * @param[in] buffer a pointer to the memory where the command will be laid out to
 * @param[in] command the command to be laid out into the buffer
 * @return the size the command takes in memory
 */
static uint32_t bulk_transfer_create_command_buffer(unsigned char *buffer, struct usbi3c_command *command)
{
	struct command_descriptor *desc = command->command_descriptor;
	uint32_t data_block_len = 0;
	uint16_t request_id;
	size_t buffer_size = 0;
	int padding = 0;

	/* command block header */
	request_id = get_request_id();
	GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(buffer)->request_id = request_id;
	if (desc->command_direction != USBI3C_READ && desc->data_length > 0) {
		/* only CCC commands or the Write command can have a data block,
		 * Read commands have a data_length > 0 because that indicates
		 * how much data to read */
		GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(buffer)->has_data = USBI3C_RESPONSE_HAS_DATA;
	} else {
		GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(buffer)->has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	}

	/* command descriptor */
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->command_type = desc->command_type;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->read_or_write = desc->command_direction;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->error_handling = desc->error_handling;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->target_address = desc->target_address;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->transfer_mode = desc->transfer_mode;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->transfer_rate = desc->transfer_rate;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->tm_specific_info = desc->tm_specific_info;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->defining_byte = desc->defining_byte;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->ccc = desc->common_command_code;
	GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer)->data_length = desc->data_length;

	/* data block */
	if (GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(buffer)->has_data && desc->data_length) {
		/* we need to pad the leading bytes of the data block
		 * with 0’s if the data block is not 32-bit aligned */
		data_block_len = get_32_bit_block_size(desc->data_length);
		padding = data_block_len - desc->data_length;
		memcpy(GET_BULK_REQUEST_DATA_BLOCK(buffer, padding), command->data, desc->data_length);
	}

	buffer_size = (BULK_REQUEST_COMMAND_BLOCK_HEADER_SIZE +
		       BULK_REQUEST_COMMAND_DESCRIPTOR_SIZE +
		       data_block_len);

	return buffer_size;
}

/**
 * @brief Creates a buffer with vendor specific data to be transferred via bulk transfer.
 *
 * @param[out] buffer a pointer to memory where the vendor specific data will be laid out to
 * @param[in] data the vendor specific data to be laid out into the buffer
 * @param[in] data_size the size of the data
 * @return the actual size the vendor specific data takes in memory after padding
 */
uint32_t bulk_transfer_create_vendor_specific_buffer(unsigned char **buffer, unsigned char *data, uint32_t data_size)
{
	uint32_t buffer_size = 0;
	uint32_t data_block_size = 0;
	int padding = 0;

	/* the data to be transferred needs to be 32-bit aligned,
	 * if it is not, we need to pad the high order bytes */
	data_block_size = get_32_bit_block_size(data_size);
	padding = data_block_size - data_size;

	/* get a buffer of a suitable size for the vendor specific request */
	buffer_size = (VENDOR_SPECIFIC_REQUEST_HEADER_SIZE + data_block_size);
	*buffer = (unsigned char *)malloc_or_die((size_t)buffer_size);

	/* create the buffer with header and vendor data */
	GET_VENDOR_SPECIFIC_REQUEST_HEADER(*buffer)->tag = VENDOR_SPECIFIC_BULK_REQUEST;
	memcpy(GET_VENDOR_SPECIFIC_BLOCK(*buffer, padding), data, data_size);

	return buffer_size;
}

/**
 * @brief Gets details and data of a vendor specific request from a response transfer buffer.
 *
 * @param[in] vendor_requests the vendor specific request tracker
 * @param[in] buffer the buffer containing the data received in the bulk transfer
 * @param[in] buffer_size the size of the buffer containing the response
 * @return 0 if the response was obtained correctly, or -1 otherwise
 */
int bulk_transfer_get_vendor_specific_response(struct vendor_specific_request *vendor_request, unsigned char *buffer, uint32_t buffer_size)
{
	unsigned char *data = NULL;
	uint32_t data_size = 0;

	/* we no longer require to keep the bulk transfer header,
	 * we only need to return the vendor specific block */
	data_size = buffer_size - BULK_TRANSFER_HEADER_SIZE;

	/* We cannot know how big the vendor specific data is going to be,
	 * the only thing we know the size of, is the bulk transfer header,
	 * the vendor specific block is defined by the vendor, so we'll just
	 * copy the whole buffer without header, and will let the vendor
	 * extract the data from there. */
	data = (unsigned char *)malloc_or_die((size_t)data_size);
	memcpy(data, (buffer + BULK_TRANSFER_HEADER_SIZE), data_size);

	/* if the user provided a callback for this kind of response, run it now */
	if (vendor_request->on_vendor_response_cb) {
		vendor_request->on_vendor_response_cb(data_size, data, vendor_request->user_data);
	}

	FREE(data);

	return 0;
}

/**
 * @brief Gets details and data of a regular command from a response transfer buffer.
 *
 * @param[in] regular_requests the regular request tracker
 * @param[in] buffer the buffer containing the data received in the bulk transfer
 * @param[in] buffer_size the size of the buffer containing the response
 * @return 0 if the response was obtained correctly, or -1 otherwise
 */
int bulk_transfer_get_regular_response(struct bulk_requests *regular_requests, unsigned char *buffer, uint32_t buffer_size)
{
	struct list *node = NULL;
	struct list *next = NULL;
	struct usbi3c_response *response = NULL;
	struct regular_request *request = NULL;
	uint16_t request_id;
	int total_commands = 0;
	int ret = 0;

	pthread_mutex_lock(regular_requests->mutex);

	buffer = buffer + BULK_TRANSFER_HEADER_SIZE;

	/* A regular bulk response transfer can have responses for one, or many
	 * commands. The request tracker in the context has information about how
	 * many commands were submitted in the same request transfer along with the
	 * request ID of the first command submitted in the transfer. We can read
	 * the ID of the first response and search for it in the request tracker to
	 * figure out the number of responses we need to read. */
	request_id = GET_BULK_RESPONSE_BLOCK_HEADER(buffer)->request_id;
	node = list_search_node(regular_requests->requests, &request_id, compare_request_id);
	if (node == NULL) {
		DEBUG_PRINT("Request ID %d is unknown\n", request_id);
		ret = -1;
		goto UNLOCK_AND_EXIT;
	}
	request = (struct regular_request *)node->data;
	total_commands = request->total_commands;

	for (int i = 0; i < total_commands; i++) {

		uint32_t response_block_size = 0;
		uint32_t data_block_size = 0;
		int padding = 0;

		/* parse the response data from the buffer */
		request_id = GET_BULK_RESPONSE_BLOCK_HEADER(buffer)->request_id;
		response = (struct usbi3c_response *)malloc_or_die(sizeof(struct usbi3c_response));
		response->has_data = GET_BULK_RESPONSE_BLOCK_HEADER(buffer)->has_data;
		response->attempted = GET_BULK_RESPONSE_BLOCK_HEADER(buffer)->attempted;

		if (response->attempted == USBI3C_COMMAND_ATTEMPTED) {
			response->error_status = GET_BULK_RESPONSE_DESCRIPTOR(buffer)->error_status;
			response->data_length = GET_BULK_RESPONSE_DESCRIPTOR(buffer)->data_length;
			response_block_size = BULK_RESPONSE_BLOCK_HEADER_SIZE + BULK_RESPONSE_DESCRIPTOR_SIZE;
		} else {
			/* the command corresponding to this request was not attempted
			 * so it has no response descriptor */
			response->error_status = 0;
			response->data_length = 0;
			response_block_size = BULK_RESPONSE_BLOCK_HEADER_SIZE;
		}

		if (response->has_data == USBI3C_RESPONSE_HAS_DATA && response->data_length > 0) {
			/* the data we got in the response is probably already 32-bit
			 * aligned, the I3C device should have already taken care of that,
			 * in order to send the data, but let's make sure anyway */
			data_block_size = get_32_bit_block_size(response->data_length);
			padding = data_block_size - response->data_length;
			response->data = (unsigned char *)malloc_or_die(response->data_length);
			memcpy(response->data, GET_BULK_RESPONSE_DATA_BLOCK(buffer, padding), response->data_length);
		} else {
			response->data = NULL;
		}

		/* node should already be pointing to the correct entry in the request tracker,
		 * but we need to make sure it does */
		if (node == NULL || node->data == NULL || ((struct regular_request *)node->data)->request_id != request_id) {
			/* as a last resort we can try finding the id in the entire
			 * list in case it got misplaced somehow */
			node = list_search_node(regular_requests->requests, &request_id, compare_request_id);
			if (!node) {
				DEBUG_PRINT("Request ID %d is unknown\n\n", request_id);
				bulk_transfer_free_response(&response);
				ret = -1;
				goto UNLOCK_AND_EXIT;
			}
		}
		next = node->next;

		/* make sure we don't already have a response for this request id */
		request = (struct regular_request *)node->data;
		if (request->response != NULL) {
			DEBUG_PRINT("A response for request ID %d already exists\n", request_id);
			bulk_transfer_free_response(&response);
			ret = -1;
			goto UNLOCK_AND_EXIT;
		}

		/* if the user added a callback to be run when the response to the command
		 * was gotten, now is the time to run it. If no callback was provided, just
		 * add the response to the tracker */
		if (request->on_response_cb) {
			ret = request->on_response_cb(response, request->user_data);
			if (ret == 0) {
				/* the response was just passed to the callback function,
				 * we no longer need to track the request */
				regular_requests->requests = list_free_node(regular_requests->requests, node, free_regular_request_in_list);
				FREE(response->data);
				FREE(response);
			} else {
				/* something must have gone wrong running the callback, so
				 * let's keep the response in the tracker */
				request->response = response;
			}
		} else {
			/* add a pointer to the response to the regular request tracker */
			request->response = response;
		}

		/* if there are more than one command responses in the transfer, they
		 * should be in order in the tracker, so just move to the next node */
		node = next;

		/* move the buffer pointer to the beginning of the next response */
		buffer = (buffer + response_block_size + data_block_size);
	}

UNLOCK_AND_EXIT:
	pthread_mutex_unlock(regular_requests->mutex);

	return ret;
}

/**
 * @brief Gets a response from the bulk response buffer.
 *
 * @param[in] context the bulk transfer context
 * @param[in] buffer the bulk response buffer to get the data from
 * @param[in] buffer_size the size of the data buffer
 */
void bulk_transfer_get_response(void *context, unsigned char *buffer, uint32_t buffer_size)
{
	struct request_tracker *request_tracker = (struct request_tracker *)context;

	/* validate data */
	if (request_tracker == NULL) {
		DEBUG_PRINT("Missing request tracker\n");
		return;
	}
	if (buffer == NULL || buffer_size == 0) {
		DEBUG_PRINT("Invalid response buffer\n");
		return;
	}

	/* let's make sure the type of response we are getting is valid */
	if (GET_BULK_TRANSFER_HEADER(buffer)->tag == INTERRUPT_BULK_RESPONSE) {
		if (ibi_response_handle(request_tracker->ibi_response_queue, buffer, buffer_size) < 0) {
			DEBUG_PRINT("Failed to handle interrupt bulk response\n");
		}
		ibi_call_pending(request_tracker->ibi);
	} else if (GET_BULK_TRANSFER_HEADER(buffer)->tag == VENDOR_SPECIFIC_BULK_REQUEST) {
		if (bulk_transfer_get_vendor_specific_response(request_tracker->vendor_request, buffer, buffer_size) < 0) {
			DEBUG_PRINT("Failed to get the vendor specific response\n");
		}
	} else if (GET_BULK_TRANSFER_HEADER(buffer)->tag == REGULAR_BULK_RESPONSE) {
		if (bulk_transfer_get_regular_response(request_tracker->regular_requests, buffer, buffer_size) < 0) {
			DEBUG_PRINT("Failed to get the regular response\n");
		}
	} else {
		DEBUG_PRINT("Unknown bulk response (Tag %x)\n", GET_BULK_TRANSFER_HEADER(buffer)->tag);
	}
}

/**
 * @brief Sends a bulk request consisting of one or many commands and their associated data.
 *
 * The commands sent by this request will be executed in strict order from first
 * to last command. The I3C function shall send a bulk response transfer containing
 * response blocks for all corresponding commands indicating success/failure. However,
 * since all transactions in USB are initiated by the host, this response has to be
 * obtained separately.
 *
 * If dependent_on_previous is set and a command in the previous bulk request stalls
 * on NACK, the execution or cancellation of the commands in this request  will rely
 * on the host sending a CANCEL_OR_RESUME_BULK_REQUEST to decide if it should retry the
 * stalled command and upon its success continue to execute subsequent dependent commands,
 * or cancel the execution of the stalled command and all subsequent dependent commands.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] commands a list of the dependent commands to be transferred
 * @param[in] dependent_on_previous indicates if these commands are dependent on the previous bulk request
 * @return a list containing the ID of each one of the submitted requests, or NULL on failure
 */
struct list *bulk_transfer_send_commands(struct usbi3c_device *usbi3c_dev, struct list *commands, uint8_t dependent_on_previous)
{
	struct regular_request *request = NULL;
	struct list *requests = NULL;
	struct list *node = NULL;
	struct list *tail = NULL;
	struct list *request_ids = NULL;
	unsigned char *buffer = NULL;
	unsigned char *cmd_buffer = NULL;
	uint32_t buffer_available = 0;
	uint32_t buffer_size = 0;
	uint32_t data_block_len = 0;
	uint32_t response_buffer_size = 0;
	uint32_t response_data_block_len = 0;
	int command_count = 0;
	int ret = -1;

	/* validate data */
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return NULL;
	}
	if (commands == NULL) {
		DEBUG_PRINT("The list of commands to transfer is missing, aborting...\n");
		return NULL;
	}
	if (dependent_on_previous != USBI3C_NOT_DEPENDENT_ON_PREVIOUS && dependent_on_previous != USBI3C_DEPENDENT_ON_PREVIOUS) {
		DEBUG_PRINT("Invalid value for dependent_on_previous, aborting...\n");
		return NULL;
	}

	/* only one bulk request transfer header is required for all commands */
	buffer_size = BULK_TRANSFER_HEADER_SIZE;

	/* we should account for buffer space needed for the current request
	 * and its corresponding response, this includes requests that will
	 * cause I3C targets to provide Read data */
	response_buffer_size = BULK_TRANSFER_HEADER_SIZE;

	for (node = commands; node; node = node->next) {

		struct usbi3c_command *command = node->data;
		struct command_descriptor *command_desc = NULL;

		if (bulk_transfer_validate_command(command) < 0) {
			return NULL;
		}
		command_desc = command->command_descriptor;

		if (command_desc->command_direction == USBI3C_READ) {
			/* 'Read' commands don't have a data block even when their
			 * data_length is > 0 */
			data_block_len = 0;
			/* however data_length in 'Read' commands indicates how much
			 * data we are expecting to read from the I3C device */
			response_data_block_len = get_32_bit_block_size(command_desc->data_length);
		} else {
			/* the command's data block (if included) should be 32-bit aligned,
			 * the data length specified by users represent number of bytes
			 * to be transferred, so they need to be padded to the closest
			 * 32-bit (4 byte) chunk */
			data_block_len = get_32_bit_block_size(command_desc->data_length);
			/* responses of 'Write' commands don't expect any data back */
			response_data_block_len = 0;
		}

		/* calculate the memory required by the current command */
		buffer_size = (buffer_size +
			       BULK_REQUEST_COMMAND_BLOCK_HEADER_SIZE +
			       BULK_REQUEST_COMMAND_DESCRIPTOR_SIZE +
			       data_block_len);

		response_buffer_size = (response_buffer_size +
					BULK_RESPONSE_BLOCK_HEADER_SIZE +
					BULK_RESPONSE_DESCRIPTOR_SIZE +
					response_data_block_len);

		command_count = command_count + 1;
	}

	/* evaluate if there is enough buffer available in the I3C function to process
	 * all the commands/data */
	ret = bulk_transfer_get_buffer_available(usbi3c_dev, &buffer_available);
	if (ret < 0) {
		DEBUG_PRINT("Could not get the buffer available from the I3C function, aborting...\n");
		return NULL;
	}
	if ((buffer_size + response_buffer_size) > buffer_available) {
		DEBUG_PRINT("There is not enough buffer available in the I3C function for the commands, aborting...\n");
		return NULL;
	}

	/* get a buffer of a suitable size for all the I3C commands in the list */
	buffer = (unsigned char *)malloc_or_die((size_t)buffer_size);
	cmd_buffer = buffer;

	/* now that we have enough memory allocated we can start building the buffer
	 * starting with the bulk request transfer header, just one for all commands */
	GET_BULK_TRANSFER_HEADER(cmd_buffer)->tag = REGULAR_BULK_REQUEST;
	GET_BULK_TRANSFER_HEADER(cmd_buffer)->dependent_on_previous = dependent_on_previous;
	cmd_buffer = (cmd_buffer + BULK_TRANSFER_HEADER_SIZE);

	/* the rest of the blocks are per command */
	for (node = commands; node; node = node->next) {

		struct usbi3c_command *command = node->data;
		uint32_t cmd_size = 0;
		uint16_t request_id;
		uint16_t *request_id_ptr = NULL;

		cmd_size = bulk_transfer_create_command_buffer(cmd_buffer, command);

		request_id = GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(cmd_buffer)->request_id;

		/* when multiple commands are sent together in a single request transfer,
		 * it means that the I3C function will execute these commands in strict order,
		 * from first to last command, and will send a response transfer containing
		 * response blocks for all corresponding command blocks.
		 * In order for us to know how many commands were in the same request transfer,
		 * we need to track the request ID of the commands, and the number of commands
		 * that were in the same transfer, that way we'll know how many responses to read.
		 *
		 * All commands in the same request transfer are dependent on the previous
		 * command from the same request transfer. The obvious exception is for the
		 * first command in the request transfer since there are no previous commands
		 * in the same request to depend on. However this request can depend on the
		 * commands from a previous request transfer. This is optionally chosen by the
		 * user via the dependent_on_previous function argument. We need to track this
		 * value so we know how to handle the commands if a previous request fails. */
		request = (struct regular_request *)malloc_or_die(sizeof(struct regular_request));
		request->request_id = request_id;
		request->total_commands = command_count;
		request->reattempt_count = 0;
		request->response = NULL;
		request->on_response_cb = command->on_response_cb;
		request->user_data = command->user_data;
		if (node == commands) {
			/* this is the first command in the request, it will depend on the commands
			 * in the previous request if the user selected it to be */
			request->dependent_on_previous = dependent_on_previous;
		} else {
			/* all subsequent commands in the request are dependent on previous by default */
			request->dependent_on_previous = TRUE;
		}

		requests = list_append(requests, request);

		/* add the request ID to the list that will be returned */
		request_id_ptr = (uint16_t *)malloc_or_die(sizeof(uint16_t));
		*request_id_ptr = request_id;
		request_ids = list_append(request_ids, request_id_ptr);

		/* move the cmd_buffer to the beginning of the next available
		 * memory space so its ready for the next command */
		cmd_buffer = (cmd_buffer + cmd_size);
	}

	/* the commands have been sent, we can add them to the request tracker now but keep
	 * a reference of the original tail in case we need to revert this */
	pthread_mutex_lock(usbi3c_dev->request_tracker->regular_requests->mutex);
	tail = list_tail(usbi3c_dev->request_tracker->regular_requests->requests);
	usbi3c_dev->request_tracker->regular_requests->requests = list_concat(usbi3c_dev->request_tracker->regular_requests->requests, requests);
	pthread_mutex_unlock(usbi3c_dev->request_tracker->regular_requests->mutex);

	/* buffer ready, the transfer can begin */
	ret = usb_output_bulk_transfer(usbi3c_dev->usb_dev, buffer, buffer_size);
	if (ret < 0) {
		DEBUG_PRINT("The commands failed to be sent\n");
		FREE(buffer);
		list_free_list_and_data(&request_ids, free);
		/* the requests list was just concatenated to the end of the requests tracker,
		 * we can just delete that part of the list and return the original list tail */
		list_free_list_and_data(&requests, free_regular_request_in_list);
		if (tail == NULL) {
			/* there was no tail, the tracker was empty */
			usbi3c_dev->request_tracker->regular_requests->requests = NULL;
		} else {
			tail->next = NULL;
		}
		return NULL;
	}

	FREE(buffer);

	return request_ids;
}

/**
 * @brief Searches for a response for a specific request id in the request tracker.
 *
 * @param[in] regular_requests the regular request tracker
 * @param[in] request_id the ID of the request to search for
 * @return the response matching the request ID if it is available, or NULL otherwise
 */
struct usbi3c_response *bulk_transfer_search_response_in_tracker(struct bulk_requests *regular_requests, int request_id)
{
	struct regular_request *request = NULL;
	struct usbi3c_response *response = NULL;
	struct list *node = NULL;

	if (regular_requests == NULL) {
		DEBUG_PRINT("Missing regular request tracker, aborting...\n");
		return NULL;
	}

	pthread_mutex_lock(regular_requests->mutex);

	if (regular_requests->requests == NULL) {
		DEBUG_PRINT("There are no requests in the tracker\n");
		goto UNLOCK_AND_EXIT;
	}

	node = list_search_node(regular_requests->requests, &request_id, compare_request_id);
	if (node == NULL) {
		/* something is wrong, a request should already exist in the tracker
		 * even without any response */
		DEBUG_PRINT("The specified request ID was not found in the regular request tracker\n");
		goto UNLOCK_AND_EXIT;
	}
	request = (struct regular_request *)node->data;

	if (request->response == NULL) {
		/* there is no response for that request yet */
		goto UNLOCK_AND_EXIT;
	}

	/* we have to copy the response data to another memory location because we are
	 * about to remove the record from the request tracker, so if we are pointing to
	 * that data we will loose it */
	response = (struct usbi3c_response *)malloc_or_die(sizeof(struct usbi3c_response));
	memcpy(response, request->response, sizeof(struct usbi3c_response));
	if (request->response->data_length > 0 && request->response->data) {
		response->data = (unsigned char *)malloc_or_die(request->response->data_length);
		memcpy(response->data, request->response->data, request->response->data_length);
	} else {
		response->data = NULL;
	}

	/* remove the request from the tracker, we no longer need to track it */
	regular_requests->requests = list_free_node(regular_requests->requests, node, free_regular_request_in_list);

UNLOCK_AND_EXIT:
	pthread_mutex_unlock(regular_requests->mutex);

	return response;
}

/**
 * @brief Allocates a usbi3c_command and pre-initializes it for you.
 *
 * When the usbi3c_command is no longer needed, it should be freed with bulk_transfer_free_command().
 * @return a newly allocated command
 */
struct usbi3c_command *bulk_transfer_alloc_command(void)
{
	struct usbi3c_command *command = NULL;

	command = (struct usbi3c_command *)malloc_or_die(sizeof(struct usbi3c_command));
	command->command_descriptor = (struct command_descriptor *)malloc_or_die(sizeof(struct command_descriptor));

	command->data = NULL;
	command->on_response_cb = NULL;
	command->user_data = NULL;

	/* default values that apply to all commands */
	command->command_descriptor->command_type = REGULAR_COMMAND;
	command->command_descriptor->error_handling = USBI3C_TERMINATE_ON_ANY_ERROR;
	command->command_descriptor->command_direction = USBI3C_WRITE;
	command->command_descriptor->target_address = 0;
	command->command_descriptor->data_length = 0;
	command->command_descriptor->transfer_mode = USBI3C_I3C_SDR_MODE;
	command->command_descriptor->transfer_rate = USBI3C_I3C_RATE_2_MHZ;
	command->command_descriptor->tm_specific_info = 0;

	/* default values for CCCs only */
	command->command_descriptor->common_command_code = 0;
	command->command_descriptor->defining_byte = 0;

	return command;
}

/**
 * @brief Determines if a request depends on another request.
 *
 * @param[in] a the request to check if it depends on another request
 * @param[in] b the request ID of the request we want to check dependency on
 * @return 0 if request 'a' depends on request 'b'
 * @return 1 if request 'a' does not depend on request 'b'
 * @return -1 if we finished searching
 */
static int is_dependent(const void *a, const void *b)
{
	struct regular_request *command = (struct regular_request *)a;
	uint16_t request_id = *(uint16_t *)b;

	if (command->request_id == request_id) {
		/* this is the command we are searching for dependency on */
		return 0;
	}

	if (command->request_id > request_id && command->dependent_on_previous == TRUE) {
		/* found a dependent */
		return 0;
	}

	if (command->request_id > request_id && command->dependent_on_previous == FALSE) {
		/* this is no longer a dependent, we are done searching */
		return -1;
	}

	/* continue the search */
	return 1;
}

/**
 * @brief Removes a command from the request tracker along with all commands that depend on it.
 *
 * @param[in] regular_requests the regular request tracker
 * @param[in] request_id the ID of the request that caused the controller to stall
 * @return 0 if the commands were removed from the request tracker successfully, or -1 otherwise
 */
int bulk_transfer_remove_command_and_dependent(struct bulk_requests *regular_requests, uint16_t request_id)
{
	if (regular_requests == NULL) {
		DEBUG_PRINT("Missing regular request tracker, aborting...\n");
		return -1;
	}

	pthread_mutex_lock(regular_requests->mutex);

	if (regular_requests->requests == NULL) {
		DEBUG_PRINT("There are no requests in the tracker\n");
		goto UNLOCK_AND_EXIT;
	}

	/* look request by request until we find the command that caused the controller
	 * to stall and remove it from the tracker, then keep looking for all requests
	 * immediately after that one with a dependent_on_previous set to TRUE, and also
	 * remove those. Once we get to the first request with dependent_on_previous set
	 * to FALSE, we are done (or if we reached the end of the list). */
	regular_requests->requests = list_free_matching_nodes(regular_requests->requests, &request_id, is_dependent, free_regular_request_in_list);

UNLOCK_AND_EXIT:
	pthread_mutex_unlock(regular_requests->mutex);

	return 0;
}

/**
 * @brief Removes a stalled command along with all commands that depend on it from the request tracker.
 *
 * This function is intended to be used as callback function when the async function
 * bulk_transfer_cancel_request_async() completes the transfer.
 *
 * @param[in] user_context the context that contains the regular_requests and request_id to be cancelled
 * @param[in] buffer ignored, not used in this function
 * @param[in] buffer_size ignored, not used in this function
 */
static void remove_stalled_commands(void *user_context, unsigned char *buffer, uint16_t buffer_size)
{
	struct cancel_stall_request_context *cancel_context = NULL;
	int ret = -1;

	cancel_context = (struct cancel_stall_request_context *)user_context;
	ret = bulk_transfer_remove_command_and_dependent(cancel_context->regular_requests, cancel_context->request_id);
	if (ret < 0) {
		DEBUG_PRINT("There was an error removing the stalled commands from the request tracker\n");
	}

	FREE(cancel_context);
}

/**
 * @brief Clears the stalled command and cancels subsequent dependent commands.
 *
 * @note This function is asynchronous.
 *
 * @param[in] usb_dev the representation of the USB device
 * @param[in] regular_requests the regular request tracker
 * @param[in] request_id the ID of the command the controller stalled on
 * @return 0 if the request is sent correctly, or -1 otherwise
 */
int bulk_transfer_cancel_request_async(struct usb_device *usb_dev, struct bulk_requests *regular_requests, uint16_t request_id)
{
	struct cancel_stall_request_context *cancel_context = NULL;
	const int NO_DATA = 0;

	if (usb_dev == NULL) {
		DEBUG_PRINT("The usb context is missing, aborting...\n");
		return -1;
	}
	if (regular_requests == NULL) {
		DEBUG_PRINT("The regular request tracker is missing, aborting...\n");
		return -1;
	}

	cancel_context = (struct cancel_stall_request_context *)malloc_or_die(sizeof(struct cancel_stall_request_context));
	cancel_context->regular_requests = regular_requests;
	cancel_context->request_id = request_id;

	return usb_output_control_transfer_async(usb_dev,
						 CANCEL_OR_RESUME_BULK_REQUEST,
						 CANCEL_BULK_REQUEST,
						 USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
						 NULL,
						 NO_DATA,
						 remove_stalled_commands,
						 (void *)cancel_context);
}

/**
 * @brief Retries the stalled command and continues with the rest of the commands if successful.
 *
 * @note This function is asynchronous.
 *
 * @param[in] usb_dev the representation of the USB device
 * @return 0 if the request is sent correctly, or -1 otherwise
 */
int bulk_transfer_resume_request_async(struct usb_device *usb_dev)
{
	const int NO_DATA = 0;

	if (usb_dev == NULL) {
		DEBUG_PRINT("The usb context is missing, aborting...\n");
		return -1;
	}

	return usb_output_control_transfer_async(usb_dev,
						 CANCEL_OR_RESUME_BULK_REQUEST,
						 RESUME_BULK_REQUEST,
						 USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
						 NULL,
						 NO_DATA,
						 NULL,
						 NULL);
}

/**
 * @brief Adds a command to the queue of commands to be transmitted to the I3C function.
 *
 * Once the desired command(s) have been queued, the commands can be transmitted by using
 * either usbi3c_send_commands() or usbi3c_submit_commands().
 *
 * @param[in] command_queue the queue holding the commands to be sent
 * @param[in] command_type the type of the command (regular, ccc, ccc with defining byte)
 * @param[in] target_address the target device address
 * @param[in] command_direction indicates the read/write direction of the command
 * @param[in] error_handling indicates the condition for the I3C controller to abort subsequent commands
 * @param[in] i3c_mode the transfer mode and rate that will be used for the transactions
 * @param[in] ccc the value for the Common Command Code (optional)
 * @param[in] defining_byte the defining byte for the CCC (optional)
 * @param[in] data the data to be transferred (optional)
 * @param[in] data_size indicates the number of bytes of data to be transferred
 * @param[in] on_response_cb a callback function to executed when a response to the command is received (optional)
 * @param[in] user_data the the data to share with the on_response_cb callback function (optional)
 * @return 0 if the command was added to the queue correctly, or -1 otherwise
 */
int bulk_transfer_enqueue_command(struct list **command_queue,
				  uint8_t command_type,
				  uint8_t target_address,
				  uint8_t command_direction,
				  uint8_t error_handling,
				  struct i3c_mode *i3c_mode,
				  uint8_t ccc,
				  uint8_t defining_byte,
				  unsigned char *data,
				  uint32_t data_size,
				  on_response_fn on_response_cb,
				  void *user_data)
{
	if (i3c_mode == NULL) {
		DEBUG_PRINT("The I3C mode is missing, aborting...\n");
		return -1;
	}
	if (data && data_size == 0) {
		DEBUG_PRINT("Data was provided but the data size is zero, aborting...\n");
		return -1;
	}
	if (command_direction != USBI3C_READ && (data == NULL && data_size > 0)) {
		DEBUG_PRINT("No data was provided but the data size is not zero, aborting...\n");
		return -1;
	}
	if (command_direction == USBI3C_READ && data != NULL) {
		DEBUG_PRINT("The 'Read' command cannot have data, aborting...\n");
		return -1;
	}
	if (command_direction == USBI3C_READ && data_size == 0) {
		DEBUG_PRINT("The 'Read' command requires the 'data_size' to specify the number of bytes to read, aborting...\n");
		return -1;
	}
	if (command_direction == USBI3C_READ && data_size % 4 != 0) {
		DEBUG_PRINT("The data size to Read has to be a multiple of 4 (32-bit aligned), aborting...\n");
		return -1;
	}
	if (ccc == 0 && defining_byte != 0) {
		DEBUG_PRINT("The CCC is missing, aborting...\n");
		return -1;
	}
	if (user_data != NULL && on_response_cb == NULL) {
		DEBUG_PRINT("User data for the callback function was provided, but no callback was provided, aborting...\n");
		return -1;
	}

	struct usbi3c_command *command = bulk_transfer_alloc_command();

	command->command_descriptor->command_type = command_type;
	command->command_descriptor->target_address = target_address;
	command->command_descriptor->command_direction = command_direction;
	command->command_descriptor->error_handling = error_handling;
	command->command_descriptor->data_length = data_size;
	command->command_descriptor->common_command_code = ccc;
	command->command_descriptor->defining_byte = defining_byte;

	/* get the transfer mode info from the context */
	command->command_descriptor->transfer_mode = i3c_mode->transfer_mode;
	command->command_descriptor->transfer_rate = i3c_mode->transfer_rate;
	command->command_descriptor->tm_specific_info = i3c_mode->tm_specific_info;

	if (data_size > 0 && data != NULL) {
		command->data = (unsigned char *)malloc_or_die(data_size);
		memcpy(command->data, data, data_size);
	} else {
		command->data = NULL;
	}

	command->on_response_cb = on_response_cb;
	command->user_data = user_data;

	*command_queue = list_append(*command_queue, command);

	return 0;
}
