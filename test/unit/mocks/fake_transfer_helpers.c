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

#include <libusb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.h"

#define ENDPOINT_SIZE 4

#define QUEUE_SIZE 32

/**
 * @struct fake_transfer_data
 * @brief Data to emit in a transfer input
 *
 * Representation of data to keep in our
 * circular queue
 */
struct fake_transfer_data {
	unsigned char *data; ///< Pointer to data to emit
	int size;	     ///< size of data to emit
};

/**
 * @struct fake_transfer_queue
 * @brief fake transfer queue to allocate transfers
 *
 * Circular queue to handle data transfer
 */
struct fake_transfer_queue {
	struct fake_transfer_data **buffer; ///< buffer of data to emit
	int front;
	int rear;
	int amount;
	int buffer_size;
};

struct fake_transfer_entry {
	struct libusb_transfer *transfer;
	int triggered;
	int status;
	struct fake_transfer_queue *queue;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexattr;
	pthread_cond_t wait_cond;
};

static pthread_mutex_t fake_transfer_table_mutex;
static struct fake_transfer_entry *fake_transfer_table[ENDPOINT_SIZE] = { (void *)0 };

static int array_queue_next_index(int index, int len)
{
	return (index + 1) % len;
}

static struct fake_transfer_queue *fake_transfer_queue_init(int buffer_size)
{
	struct fake_transfer_queue *transfer_queue = calloc(1, sizeof(struct fake_transfer_queue));
	transfer_queue->buffer = calloc(1, sizeof(struct fake_transfer_data *) * buffer_size);
	transfer_queue->buffer_size = buffer_size;
	return transfer_queue;
}

static int fake_transfer_queue_enqueue(struct fake_transfer_queue *transfer_queue, struct fake_transfer_data *data)
{
	if (transfer_queue->amount >= transfer_queue->buffer_size) {
		return -1;
	}
	transfer_queue->buffer[transfer_queue->rear] = data;
	transfer_queue->rear = array_queue_next_index(transfer_queue->rear, transfer_queue->buffer_size);
	transfer_queue->amount++;
	return 0;
}

static struct fake_transfer_data *fake_transfer_queue_dequeue(struct fake_transfer_queue *transfer_queue)
{
	if (transfer_queue->amount == 0) {
		return NULL;
	}
	struct fake_transfer_data *data = transfer_queue->buffer[transfer_queue->front];
	transfer_queue->front = array_queue_next_index(transfer_queue->front, transfer_queue->buffer_size);
	transfer_queue->amount--;
	return data;
}

static void fake_transfer_queue_deinit(struct fake_transfer_queue **transfer_queue)
{
	free((*transfer_queue)->buffer);
	free(*transfer_queue);
	*transfer_queue = NULL;
}

static struct fake_transfer_data *create_fake_transfer_data(unsigned char *data, int size)
{
	struct fake_transfer_data *transfer_data = (struct fake_transfer_data *)malloc(sizeof(struct fake_transfer_data));
	transfer_data->data = data;
	transfer_data->size = size;
	return transfer_data;
}

static void destroy_transfer_data(struct fake_transfer_data **transfer_data)
{
	free(*transfer_data);
	*transfer_data = NULL;
}

static void fake_transfer_entry_init(int endpoint)
{
	struct fake_transfer_entry *fake_transfer = calloc(1, sizeof(struct fake_transfer_entry));
	fake_transfer->queue = fake_transfer_queue_init(QUEUE_SIZE);
	pthread_mutexattr_init(&fake_transfer->mutexattr);
	pthread_mutexattr_settype(&fake_transfer->mutexattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&fake_transfer->mutex, &fake_transfer->mutexattr);
	pthread_cond_init(&fake_transfer->wait_cond, NULL);
	fake_transfer_table[endpoint] = fake_transfer;
	fake_transfer_table[endpoint]->status = LIBUSB_TRANSFER_COMPLETED;
}

static void fake_transfer_entry_deinit(int endpoint)
{
	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	struct fake_transfer_data *data = NULL;
	while ((data = fake_transfer_queue_dequeue(fake_transfer_table[endpoint]->queue)))
		destroy_transfer_data(&data);
	fake_transfer_queue_deinit(&fake_transfer_table[endpoint]->queue);
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
	pthread_mutex_destroy(&fake_transfer_table[endpoint]->mutex);
	pthread_mutexattr_destroy(&fake_transfer_table[endpoint]->mutexattr);
	pthread_cond_destroy(&fake_transfer_table[endpoint]->wait_cond);
	free(fake_transfer_table[endpoint]);
	fake_transfer_table[endpoint] = NULL;
}

static int fake_transfer_check_endpoint_initiated(int endpoint)
{
	int initiated = 0;
	pthread_mutex_lock(&fake_transfer_table_mutex);
	initiated = fake_transfer_table[endpoint] == NULL;
	pthread_mutex_unlock(&fake_transfer_table_mutex);
	return initiated;
}

/**
 * @brief Initialize fake transfer table.
 *
 *  Initialization of fake transfer table which is going
 *  to initialize fake transfer entry which index is the
 *  transfer endpoint number without direction, also starts
 *  table mutex and thread condition variable.
 */
void fake_transfer_init(void)
{
	pthread_mutex_init(&fake_transfer_table_mutex, NULL);
	for (int i = 0; i < ENDPOINT_SIZE; i++)
		fake_transfer_entry_init(i);
}

/**
 * @brief De-initialize fake transfer table
 *
 *  De-initialize fake transfer table destroying
 *  fake transfer entry, table mutex and table thread
 *  conditional variable.
 */
void fake_transfer_deinit(void)
{
	pthread_mutex_destroy(&fake_transfer_table_mutex);
	for (int i = 0; i < ENDPOINT_SIZE; i++)
		if (fake_transfer_table[i] != NULL) {
			fake_transfer_entry_deinit(i);
		}
}

/**
 * @brief Set transfer status
 *
 *  Set the transfer status to be sent in the next
 *  transaction, no data needed.
 *
 * @param[in] endpoint Transaction endpoint number
 * @param[in] status Transaction status
 */
void fake_transfer_set_transaction_status(int endpoint, int status)
{
	if (fake_transfer_check_endpoint_initiated(endpoint)) {
		return;
	}
	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	fake_transfer_table[endpoint]->status = status;
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
}

/**
 * @brief Add transfer to fake transfer table
 *
 *  This function add a libusb transfer to the
 *  fake transfer table in the position given
 *  as endpoint.
 *
 * @param[in] endpoint Transaction endpoint number
 * @param[in] transfer libusb_transfer to add to fake transfer table
 */
void fake_transfer_add_transfer(int endpoint, struct libusb_transfer *transfer)
{
	if (fake_transfer_check_endpoint_initiated(endpoint)) {
		return;
	}
	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	fake_transfer_table[endpoint]->transfer = transfer;
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
}

/**
 * @brief Get transfer from fake transfer table
 *
 *  Get libusb_transfer from fake transfer table
 *  if there is one assigned to the endpoint position
 *
 * @param[in] endpoint Endpoint to find libusb_transfer on fake transfer table
 * @return libusb_transfer on endpoint position or NULL if table is not initialized or endpoint transfer has not been assigned
 */
struct libusb_transfer *fake_transfer_get_transfer(int endpoint)
{
	if (fake_transfer_check_endpoint_initiated(endpoint)) {
		return NULL;
	}
	struct libusb_transfer *transfer = NULL;
	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	transfer = fake_transfer_table[endpoint]->transfer;
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
	return transfer;
}

/**
 * @brief Add data to be transferred
 *
 *  Add data to be transferred by the USB transfer
 *  assign to the given endpoint.
 *
 * @param[in] endpoint Endpoint that define the kind of transfer
 * @param[in] data Data to emit with the given transfer
 * @param[in] size size of the buffer that is going to be emitted
 */
void fake_transfer_add_data(int endpoint, unsigned char *data, int size)
{
	if (fake_transfer_check_endpoint_initiated(endpoint)) {
		return;
	}
	struct fake_transfer_data *transfer_data = create_fake_transfer_data(data, size);
	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	fake_transfer_queue_enqueue(fake_transfer_table[endpoint]->queue, transfer_data);
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
}

static void fake_transfer_emit_transfer_data(struct fake_transfer_entry *fake_transfer)
{
	if (fake_transfer == NULL) {
		return;
	}
	pthread_mutex_lock(&fake_transfer->mutex);
	if (!fake_transfer->triggered) {
		goto MUTEX_UNLOCK;
	}

	if (fake_transfer->transfer == NULL || fake_transfer->transfer->callback == NULL) {
		goto MUTEX_UNLOCK;
	}

	if (fake_transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fake_transfer->transfer->status = fake_transfer->status;
		fake_transfer->transfer->callback(fake_transfer->transfer);
		goto POST_CALLBACK;
	}

	struct fake_transfer_data *data = fake_transfer_queue_dequeue(fake_transfer->queue);
	if (data == NULL) {
		goto MUTEX_UNLOCK;
	}
	int endpoint = fake_transfer->transfer->endpoint & USB_ENDPOINT_MASK;
	int offset = 0;
	if (endpoint == USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX) {
		offset = sizeof(struct libusb_control_setup);
	}
	fake_transfer->transfer->length = data->size + offset;
	fake_transfer->transfer->actual_length = data->size;
	memcpy(fake_transfer->transfer->buffer + offset, data->data, data->size);

	fake_transfer->transfer->callback(fake_transfer->transfer);
	destroy_transfer_data(&data);

POST_CALLBACK:
	// releasing thread that trigger for this transfer type
	pthread_cond_signal(&fake_transfer->wait_cond);
	fake_transfer->triggered = 0;
	fake_transfer->status = LIBUSB_TRANSFER_COMPLETED;

MUTEX_UNLOCK:
	pthread_mutex_unlock(&fake_transfer->mutex);
}

/**
 * @brief Emit transfer from fake transfer table
 *
 *  Emit transfer from fake transfer table if there is
 *  data and trigger function has been called, this function
 *  check in every endpoint if its transaction has been triggered.
 *
 *  This function is called on libusb_handle_events mock.
 *
 */
void fake_transfer_emit(void)
{
	for (int i = 0; i < ENDPOINT_SIZE; i++) {
		if (fake_transfer_check_endpoint_initiated(i)) {
			continue;
		}
		fake_transfer_emit_transfer_data(fake_transfer_table[i]);
	}
}

/**
 * @brief Trigger endpoint kind transfer
 *
 *  Trigger an endpoint kind transfer if there is
 *  a transaction and data assign to that endpoint.
 *
 * @param[in] endpoint Endpoint transfer type to fake transfer
 */
void fake_transfer_trigger(int endpoint)
{
	if (fake_transfer_check_endpoint_initiated(endpoint)) {
		return;
	}

	pthread_mutex_lock(&fake_transfer_table[endpoint]->mutex);
	fake_transfer_table[endpoint]->triggered = 1;
	// waiting until the transfer is completed
	pthread_cond_wait(&fake_transfer_table[endpoint]->wait_cond, &fake_transfer_table[endpoint]->mutex);
	pthread_mutex_unlock(&fake_transfer_table[endpoint]->mutex);
}
