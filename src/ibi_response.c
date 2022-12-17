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

#include <pthread.h>
#include <string.h>
#include <time.h>

#include "ibi_response_i.h"
#include "usbi3c_i.h"

/**
 * @brief Minimal data received in an IBI response
 */
struct payload_buffer_entry {
	uint8_t *buffer; ///< Buffer that contains data received from IBI response
	size_t size;	 ///< Size in bytes of data received from IBI response
};

/**
 * @brief Buffer to collect IBI response data that correspond to the same IBI
 */
struct ibi_payload_buffer {
	struct list *head;   ///< List of collected data
	size_t payload_size; ///< Size in bytes of the collected data
};

struct ibi_payload_buffer payload_buffer = {
	.head = NULL,
	.payload_size = 0,
};

/**
 * @struct ibi_response_queue
 * @brief A queue of IBI responses.
 */
struct ibi_response_queue {
	struct list *head;	///< The list of IBI responses.
	size_t data_queue_size; //< The length of the data in the queue.
};

// ibi response queue
static struct ibi_response_queue response_queue = {
	.head = NULL,
	.data_queue_size = 0,
};

static void ibi_payload_buffer_enqueue(struct ibi_payload_buffer *buffer,
				       uint8_t *payload, size_t payload_size)
{
	struct payload_buffer_entry *entry;

	entry = malloc_or_die(sizeof(struct payload_buffer_entry));

	entry->buffer = payload;
	entry->size = payload_size;

	buffer->head = list_append(buffer->head, entry);
	buffer->payload_size += payload_size;
}

static void free_payload_buffer_entry_and_data(void *entry)
{
	struct payload_buffer_entry *e = entry;
	FREE(e->buffer);
	FREE(entry);
}

static void ibi_payload_buffer_cleanup(struct ibi_payload_buffer *buffer)
{
	list_free_list_and_data(&buffer->head, free_payload_buffer_entry_and_data);
	buffer->payload_size = 0;
}

static int ibi_payload_buffer_join(struct ibi_payload_buffer *buffer, uint8_t **payload)
{
	struct payload_buffer_entry *entry;
	struct list *current = buffer->head;

	if (!buffer->head || !buffer->payload_size) {
		*payload = NULL;
		return 0;
	}

	uint8_t *payload_buffer = malloc_or_die(buffer->payload_size);
	size_t offset = 0;
	while (current) {
		entry = current->data;
		payload_buffer[offset] = *entry->buffer;
		offset += entry->size;
		current = current->next;
	}
	*payload = payload_buffer;
	ibi_payload_buffer_cleanup(buffer);
	return offset;
}

/**
 * @brief Create a new IBI response queue.
 *
 * function to create a new IBI response queue.
 * @return A pointer to the new IBI response queue.
 */
struct ibi_response_queue *ibi_response_queue_get_queue(void)
{
	return &response_queue;
}

/**
 * @brief Enqueue an IBI response.
 *
 * function to enqueue an IBI response.
 * @param[in] queue The IBI response queue.
 * @param[in] response The IBI response to enqueue.
 * @return 0 on success, -1 on error.
 */
int ibi_response_queue_enqueue(struct ibi_response_queue *queue, struct ibi_response *response)
{
	if (queue == NULL || response == NULL) {
		return -1;
	}
	queue->head = list_append(queue->head, response);
	queue->data_queue_size++;
	return 0;
}

/**
 * @brief Dequeue an IBI response.
 *
 * function to dequeue an IBI response.
 * @param[in] queue The IBI response queue.
 * @return A pointer to the dequeued IBI response.
 * @note The IBI response is allocated.
 * @note The IBI response is removed from the queue.
 */
struct ibi_response *ibi_response_queue_dequeue(struct ibi_response_queue *queue)
{
	if (queue == NULL) {
		return NULL;
	}
	if (queue->head == NULL) {
		return NULL;
	}
	struct list *node = queue->head;
	void *data = node->data;
	queue->head = node->next;
	FREE(node);
	queue->data_queue_size--;
	return data;
}

/**
 * @brief Get first IBI response from the queue.
 *
 * Get first IBI response from the queue without removing it.
 *
 * @param[in] queue The IBI response queue.
 * @return first IBI response from the queue or NULL if queue is empty.
 */
struct ibi_response *ibi_response_queue_front(struct ibi_response_queue *queue)
{
	if (queue == NULL) {
		return NULL;
	}
	if (queue->head == NULL) {
		return NULL;
	}
	return queue->head->data;
}

/**
 * @brief Get last IBI response from the queue.
 *
 * Get last IBI response from the queue without removing it.
 *
 * @param[in] queue The IBI response queue.
 * @return last IBI response from the queue or NULL if queue is empty.
 */
struct ibi_response *ibi_response_queue_back(struct ibi_response_queue *queue)
{
	if (queue == NULL) {
		return NULL;
	}

	struct list *tail = list_tail(queue->head);

	if (tail == NULL) {
		return NULL;
	}

	return tail->data;
}

/**
 * @brief Get the length of the queue.
 *
 *  Get the length of the queue.
 *
 * @param[in] queue The IBI response queue.
 * @return the length of the queue.
 */
size_t ibi_response_queue_size(struct ibi_response_queue *queue)
{
	if (queue == NULL) {
		return 0;
	}
	return queue->data_queue_size;
}

static void ibi_response_fill_descriptor(struct ibi_response *response, uint8_t *data, size_t size)
{
	struct bulk_ibi_response_footer *footer = (struct bulk_ibi_response_footer *)(data + size - sizeof(struct bulk_ibi_response_footer));
	response->descriptor.address = footer->target_address;
	response->descriptor.R_W = footer->R_W;
	response->descriptor.ibi_status = footer->ibi_status;
	response->descriptor.error = footer->error;
	response->descriptor.ibi_timestamp = footer->ibi_timestamp;
	response->descriptor.ibi_type = footer->ibi_type;
	response->descriptor.MDB = (uint8_t) * (data + sizeof(struct bulk_ibi_response_header));
}

/**
 * @brief Function to handle IBI response
 *
 *  Function to handle an IBI response collecting its payload data and queuing it
 *
 * @param[in] queue queue to store IBI response data until its completed and its notification is triggered
 * @param[in] data data content of IBI response
 * @param[in] size the size of the IBI response
 * @return Return parameter description
 */
int ibi_response_handle(struct ibi_response_queue *queue, uint8_t *data, size_t size)
{
	if (queue == NULL || data == NULL || size == 0) {
		return -1;
	}

	if (size < (sizeof(struct bulk_ibi_response_header) + sizeof(struct bulk_ibi_response_footer))) {
		return -1;
	}

	struct bulk_ibi_response_header *header = (struct bulk_ibi_response_header *)data;
	struct bulk_ibi_response_footer *footer = (struct bulk_ibi_response_footer *)(data + (size - sizeof(struct bulk_ibi_response_footer)));

	if (header->sequence_id == 0) {

		if (payload_buffer.payload_size > 0) {
			DEBUG_PRINT("Payload buffer not empty, some data has been lost\n");
			ibi_payload_buffer_cleanup(&payload_buffer);
		}

		struct ibi_response *response = malloc_or_die(sizeof(struct ibi_response));
		ibi_response_fill_descriptor(response, data, size);
		ibi_response_queue_enqueue(queue, response);
	}

	if (footer->pending_read) {

		size_t payload_size = size - (sizeof(struct bulk_ibi_response_footer) + sizeof(struct bulk_ibi_response_header));

		if (footer->bytes_valid) {
			payload_size -= DWORD_SIZE;
			payload_size += footer->bytes_valid;
		}

		uint8_t *buffer = malloc_or_die(payload_size);
		memcpy(buffer, data + sizeof(struct bulk_ibi_response_header), payload_size);
		ibi_payload_buffer_enqueue(&payload_buffer, buffer, payload_size);
	}

	if (footer->last_byte) {
		struct ibi_response *response = ibi_response_queue_back(queue);

		if (response == NULL) {
			DEBUG_PRINT("Last byte received but no response in queue\n");
			return -1;
		}

		response->size = ibi_payload_buffer_join(&payload_buffer, &response->data);

		response->completed = TRUE;
	}

	return 0;
}

static void ibi_response_free(void *data)
{
	struct ibi_response *response = (struct ibi_response *)data;

	if (response->data != NULL) {
		FREE(response->data);
	}
	FREE(response);
}

/**
 * @brief Cleanup the IBI response queue.
 *
 *  Cleanup the IBI response queue.
 *
 * @param[in] queue The IBI response queue.
 */
void ibi_response_queue_clear(struct ibi_response_queue *queue)
{
	if (queue == NULL) {
		return;
	}

	list_free_list_and_data(&queue->head, ibi_response_free);
	queue->data_queue_size = 0;
}
