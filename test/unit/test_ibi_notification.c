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
#include "ibi_i.h"
#include "mocks.h"

struct test_deps {
	struct ibi_response_queue *queue;
};

int setup(void **state)
{
	struct test_deps *deps = calloc(1, sizeof(struct test_deps));
	deps->queue = ibi_response_queue_get_queue();
	*state = deps;
	return 0;
}

int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	free(deps);
	return 0;
}

static void test_negative_ibi_init_null_pointers(void **state)
{
	assert_null(ibi_init(NULL));
}

static void test_ibi_init_success(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct ibi *ibi = ibi_init(deps->queue);
	assert_non_null(ibi);
	ibi_destroy(&ibi);
}

void ibi_callback(uint8_t report, struct usbi3c_ibi *descriptor, uint8_t *data, size_t size, void *user_data)
{
	check_expected(report);
	check_expected(descriptor);
	check_expected(data);
	check_expected(size);
	check_expected(user_data);
}

static void test_ibi_set_callback_null_pointers(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct ibi *ibi = ibi_init(deps->queue);
	ibi_set_callback(ibi, NULL, NULL);
	ibi_set_callback(NULL, ibi_callback, NULL);
	ibi_set_callback(NULL, NULL, NULL);
	ibi_destroy(&ibi);
}

static void test_ibi_notification_handler(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct notification notification = {
		.type = NOTIFICATION_I3C_IBI,
		.code = REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER
	};
	struct ibi *ibi = ibi_init(deps->queue);
	assert_non_null(ibi);
	ibi_handle_notification(&notification, ibi);
	ibi_destroy(&ibi);
}

static void test_negative_ibi_call_pending_null_param(void **state)
{
	ibi_call_pending(NULL);
}

static void test_negative_ibi_call_pending_without_interrupts_or_responses(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct ibi *ibi = ibi_init(deps->queue);
	ibi_set_callback(ibi, ibi_callback, NULL);
	ibi_call_pending(ibi);
	ibi_destroy(&ibi);
}

static void test_negative_ibi_call_pending_with_not_completed_responses(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct notification notification = {
		.type = NOTIFICATION_I3C_IBI,
		.code = REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER
	};
	struct ibi *ibi = ibi_init(deps->queue);
	assert_non_null(ibi);

	ibi_handle_notification(&notification, ibi);

	struct ibi_response *response = calloc(1, sizeof(struct ibi_response));
	ibi_response_queue_enqueue(deps->queue, response);
	ibi_call_pending(ibi);
	ibi_response_queue_dequeue(deps->queue);
	free(response);
	ibi_destroy(&ibi);
}

static void test_negative_ibi_call_pending_no_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct notification notification = {
		.type = NOTIFICATION_I3C_IBI,
		.code = REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER
	};
	struct ibi *ibi = ibi_init(deps->queue);
	assert_non_null(ibi);

	ibi_handle_notification(&notification, ibi);

	struct ibi_response *response = calloc(1, sizeof(struct ibi_response));
	response->completed = 1;
	ibi_response_queue_enqueue(deps->queue, response);
	ibi_call_pending(ibi);
	ibi_destroy(&ibi);
}

static void test_negative_ibi_call_pending(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t data = 0xBE;
	struct notification notification = {
		.type = NOTIFICATION_I3C_IBI,
		.code = REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER
	};
	struct ibi *ibi = ibi_init(deps->queue);
	assert_non_null(ibi);
	ibi_set_callback(ibi, ibi_callback, &data);

	ibi_handle_notification(&notification, ibi);

	struct ibi_response *response = calloc(1, sizeof(struct ibi_response));
	response->completed = 1;
	response->data = calloc(1, sizeof(uint32_t));
	int32_t payload = 0xBADBEEF;
	memcpy(response->data, &payload, sizeof(payload));
	response->size = sizeof(uint32_t);
	ibi_response_queue_enqueue(deps->queue, response);

	expect_value(ibi_callback, report, REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER);
	expect_memory(ibi_callback, data, &payload, sizeof(payload));
	expect_value(ibi_callback, size, 4);
	expect_value(ibi_callback, descriptor, &response->descriptor);
	expect_value(ibi_callback, user_data, &data);
	ibi_call_pending(ibi);
	ibi_destroy(&ibi);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_ibi_init_null_pointers, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_init_success, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_set_callback_null_pointers, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ibi_notification_handler, setup, teardown),
		cmocka_unit_test(test_negative_ibi_call_pending_null_param),
		cmocka_unit_test_setup_teardown(test_negative_ibi_call_pending_without_interrupts_or_responses, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_ibi_call_pending_with_not_completed_responses, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_ibi_call_pending_no_callback, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_ibi_call_pending, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
