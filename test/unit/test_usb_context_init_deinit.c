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
#include "mocks.h"

#include "usb_i.h"

/* test usb_context initialization handling libusb_init failure */
static void test_negative_usb_context_init_fail(void **state)
{
	struct usb_context *usb_ctx = NULL;
	mock_libusb_init(NULL, RETURN_FAILURE);
	int ret = usb_context_init(&usb_ctx);
	assert_null(usb_ctx);
	assert_int_equal(ret, RETURN_FAILURE);
}

int mutex_return = 0;
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr)
{
	return mutex_return;
}

/* test usb_context initialization handling pthread_mutex_init failure */
static void test_negative_usb_context_init_mutex_fail(void **state)
{
	struct usb_context *usb_ctx = NULL;
	mock_libusb_init(NULL, RETURN_SUCCESS);
	mutex_return = RETURN_FAILURE;
	int ret = usb_context_init(&usb_ctx);
	assert_null(usb_ctx);
	assert_int_equal(ret, RETURN_FAILURE);
}

int pthread_return = 0;
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	return pthread_return;
}

/* test usb_context initialization handling pthread_create failure */
static void test_negative_usb_context_init_thread_fail(void **state)
{
	struct usb_context *usb_ctx = NULL;
	mock_libusb_init(NULL, RETURN_SUCCESS);
	mutex_return = RETURN_SUCCESS;
	pthread_return = RETURN_FAILURE;
	int ret = usb_context_init(&usb_ctx);
	assert_null(usb_ctx);
	assert_int_equal(ret, RETURN_FAILURE);
}

int pthread_join(pthread_t thread, void **retval)
{
	return 0;
}

/* test usb_context initialization success */
static void test_usb_context_init_deinit(void **state)
{
	struct usb_context *usb_ctx = NULL;
	mock_libusb_init(NULL, RETURN_SUCCESS);
	mutex_return = RETURN_SUCCESS;
	pthread_return = RETURN_SUCCESS;
	int ret = usb_context_init(&usb_ctx);
	assert_int_equal(ret, RETURN_SUCCESS);

	usb_context_deinit(usb_ctx);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_negative_usb_context_init_fail),
		cmocka_unit_test(test_negative_usb_context_init_mutex_fail),
		cmocka_unit_test(test_negative_usb_context_init_thread_fail),
		cmocka_unit_test(test_usb_context_init_deinit),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
