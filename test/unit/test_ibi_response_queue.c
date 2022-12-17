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
#include "ibi_response_i.h"
#include "mocks.h"

static struct ibi_response_queue *queue;

static int setup(void **state)
{
	queue = ibi_response_queue_get_queue();
	return 0;
}

static int teardown(void **state)
{
	ibi_response_queue_clear(queue);
	return 0;
}

// Test that the queue is reachable
static void test_ibi_response_queue_get_queue(void **state)
{
	assert_non_null(ibi_response_queue_get_queue());
}

// Test ibi_response_queue_clear is handling null pointers
static void test_ibi_response_queue_clear_null(void **state)
{
	ibi_response_queue_clear(NULL);
}

// Test that the queue in handling null pointers
static void test_ibi_response_queue_enqueue_null(void **state)
{
	struct ibi_response response;
	assert_int_equal(ibi_response_queue_enqueue(NULL, NULL), RETURN_FAILURE);
	assert_int_equal(ibi_response_queue_enqueue(queue, NULL), RETURN_FAILURE);
	assert_int_equal(ibi_response_queue_size(queue), 0);
	assert_int_equal(ibi_response_queue_enqueue(NULL, &response), RETURN_FAILURE);
}

// Test that the queue is queueing responses correctly
static void test_ibi_response_queue_enqueue(void **state)
{
	struct ibi_response response;
	assert_int_equal(ibi_response_queue_enqueue(queue, &response), RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_true(&response == ibi_response_queue_front(queue));
	ibi_response_queue_dequeue(queue);
}

// test that the queue is dequeueing multiple responses correctly
static void test_ibi_response_queue_enqueue_multiple(void **state)
{
	struct ibi_response response;
	struct ibi_response response2;
	assert_int_equal(ibi_response_queue_enqueue(queue, &response), RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_enqueue(queue, &response2), RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 2);
	assert_true(&response == ibi_response_queue_front(queue));
	ibi_response_queue_dequeue(queue);
	ibi_response_queue_dequeue(queue);
}

// test that the queue is handling null pointers on dequeue
static void test_ibi_response_queue_dequeue_null(void **state)
{
	assert_null(ibi_response_queue_dequeue(NULL));
}

// test that the queue is dequeueing responses correctly when there are no responses in the queue
static void test_ibi_response_queue_dequeue_empty(void **state)
{
	assert_null(ibi_response_queue_dequeue(queue));
}

// test that the queue is dequeueing responses correctly and that the queue is empty afterwards
static void test_ibi_response_queue_dequeue(void **state)
{
	struct ibi_response response;
	assert_int_equal(ibi_response_queue_enqueue(queue, &response), RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_true(&response == ibi_response_queue_front(queue));
	assert_true(&response == ibi_response_queue_dequeue(queue));
	assert_int_equal(ibi_response_queue_size(queue), 0);
}

// test that the queue from returns null if the parameter is null or the queue is empty
static void test_ibi_response_queue_front_null(void **state)
{
	assert_null(ibi_response_queue_front(NULL));
	assert_null(ibi_response_queue_front(queue));
}

// test that the queue back returns null if the parameter is null or the queue is empty
static void test_ibi_response_queue_back_null(void **state)
{
	assert_null(ibi_response_queue_back(NULL));
	assert_null(ibi_response_queue_back(queue));
}

// test that the queue back returns the last response in the queue
static void test_ibi_response_queue_back(void **state)
{
	struct ibi_response response;
	assert_int_equal(ibi_response_queue_enqueue(queue, &response), RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_true(&response == ibi_response_queue_back(queue));
	ibi_response_queue_dequeue(queue);
}

// test that the queue size returns 0 if the parameter is null
static void test_ibi_response_queue_size_null(void **state)
{
	assert_int_equal(ibi_response_queue_size(NULL), 0);
}

// Header macros
#define RESPONSE_TYPE_IBI (1)
#define SET_SEQUENCE_ID(id) (((uint32_t)(id)) << 16)

// Footer macros
#define PENDING_READ (1 << 12)
#define LAST_BYTE (1 << 13)
#define SET_VALID_BYTES(bytes) ((uint32_t)(bytes) << 14)

// Create IBI response buffer
static int create_response_buffer(uint8_t **buffer_out, uint16_t sequence_id, uint32_t footer, uint8_t *payload_content, size_t payload_size)
{
	uint32_t *dword_ptr = NULL;
	uint32_t buffer_size;

	// add the size of the payload aligned to 4 bytes
	size_t aligned_payload_size = ((payload_size + DWORD_SIZE - 1) & ~(DWORD_SIZE - 1));

	// calculate the size of the buffer aligned to 4 bytes
	if (aligned_payload_size > 0) {
		buffer_size = DWORD_SIZE + aligned_payload_size + DWORD_SIZE;
	} else {
		buffer_size = 4 * DWORD_SIZE;
	}
	*buffer_out = (unsigned char *)calloc(1, (size_t)buffer_size);
	assert_non_null(*buffer_out);

	dword_ptr = (uint32_t *)*buffer_out;

	*dword_ptr++ = RESPONSE_TYPE_IBI | SET_SEQUENCE_ID(sequence_id); // Interrupt response = 0x1
	if (payload_size > 0) {
		memcpy(dword_ptr, payload_content, payload_size);
		dword_ptr += (aligned_payload_size / DWORD_SIZE);
	} else {
		*dword_ptr++ = 0x00000000; // MDB = 0x0 && data if exists
		*dword_ptr++ = 0x00000000; // No data
	}
	*dword_ptr++ = footer | SET_VALID_BYTES(payload_size % DWORD_SIZE); // IBI response footer

	return buffer_size;
}

// test ibi_response handle is handling null pointers properly
static void test_ibi_response_handle_null(void **state)
{
	uint8_t fake_buffer = 1;
	assert_int_equal(ibi_response_handle(NULL, &fake_buffer, 1), RETURN_FAILURE);
	assert_int_equal(ibi_response_handle(queue, NULL, 0), RETURN_FAILURE);
	assert_int_equal(ibi_response_handle(NULL, NULL, 0), RETURN_FAILURE);
}

// test ibi_response handle buffers with small sizes
static void test_ibi_response_handle_small_buffer(void **state)
{
	uint32_t buffer = 0x01;
	uint32_t buffer_size = sizeof(buffer);
	int ret = 0;

	ret = ibi_response_handle(queue, (uint8_t *)&buffer, buffer_size);
	assert_int_equal(ret, RETURN_FAILURE);
}

// test ibi response handler receiving a response without payload
static void test_ibi_response_handler_no_payload(void **state)
{
	// create response buffer with no payload
	uint8_t *buffer = NULL;
	int ret = 0;
	uint16_t sequence_id = 0;
	uint32_t footer = LAST_BYTE;

	size_t buffer_size = create_response_buffer(&buffer, sequence_id, footer, NULL, 0);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	struct ibi_response *response = ibi_response_queue_back(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_int_equal(response->size, 0);
	assert_true(response->completed);
	free(buffer);
}

// test ibi response handler receiving a response with payload
static void test_ibi_response_handler_with_payload(void **state)
{
	uint8_t *buffer = NULL;
	int ret = 0;
	uint16_t sequence_id = 0;
	uint32_t footer = 0;

	// create response with sequence id 0 and no payload
	size_t buffer_size = create_response_buffer(&buffer, sequence_id, footer, NULL, 0);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	struct ibi_response *response = ibi_response_queue_back(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_int_equal(response->size, 0);
	assert_false(response->completed);
	free(buffer);

	sequence_id = 1;
	uint8_t payload_content = 0xEA;
	size_t payload_size = 1;
	footer = LAST_BYTE | PENDING_READ;

	// create response with sequence id 1 and payload
	buffer_size = create_response_buffer(&buffer, sequence_id, footer, &payload_content, payload_size);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	response = ibi_response_queue_back(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_int_equal(response->size, 1);
	assert_true(response->completed);
	free(buffer);
}

// test ibi_response_handle getting a initial response without finish the current response
static void test_ibi_response_handler_multiple_initial_responses(void **state)
{
	uint8_t *buffer = NULL;
	int ret = 0;
	uint16_t sequence_id = 0;
	uint32_t footer = 0;

	// create response with sequence id 0 and no payload
	size_t buffer_size = create_response_buffer(&buffer, sequence_id, footer, NULL, 0);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	struct ibi_response *response = ibi_response_queue_back(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_int_equal(response->size, 0);
	assert_false(response->completed);
	free(buffer);

	sequence_id = 1;
	uint32_t payload_content = 0xDEADBEEF;
	size_t payload_size = 3;
	footer = PENDING_READ;

	// create response with sequence id 1 and payload
	buffer_size = create_response_buffer(&buffer, sequence_id, footer, (uint8_t *)&payload_content, payload_size);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	response = ibi_response_queue_back(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 1);
	assert_int_equal(response->size, 0);
	assert_false(response->completed);
	free(buffer);

	buffer = NULL;
	sequence_id = 0;
	footer = 0;

	// create response with sequence id 0 and no payload
	buffer_size = create_response_buffer(&buffer, sequence_id, footer, NULL, 0);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	response = ibi_response_queue_front(queue);

	assert_int_equal(ret, RETURN_SUCCESS);
	assert_int_equal(ibi_response_queue_size(queue), 2);
	assert_int_equal(response->size, 0);
	assert_false(response->completed);
	free(buffer);
}

// test negative ibi_response_handle getting a last byte response without have a initial response
static void test_ibi_response_handler_last_byte_without_initial_response(void **state)
{
	uint8_t *buffer = NULL;
	int ret = 0;
	uint32_t payload_content = 0x0BADBEEF;
	size_t payload_size = 3;
	uint16_t sequence_id = 1;
	uint32_t footer = LAST_BYTE;

	// create response with sequence id 0 and no payload
	size_t buffer_size = create_response_buffer(&buffer, sequence_id, footer, (uint8_t *)&payload_content, payload_size);

	// handle response
	ret = ibi_response_handle(queue, buffer, buffer_size);

	// check response
	assert_int_equal(ret, RETURN_FAILURE);
	assert_int_equal(ibi_response_queue_size(queue), 0);
	free(buffer);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ibi_response_queue_get_queue),
		cmocka_unit_test(test_ibi_response_queue_clear_null),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_enqueue_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_enqueue, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_enqueue_multiple, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_dequeue_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_dequeue_empty, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_dequeue, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_size_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_front_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_back_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_queue_back, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handle_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handle_small_buffer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handler_no_payload, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handler_with_payload, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handler_multiple_initial_responses, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_response_handler_last_byte_without_initial_response, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
