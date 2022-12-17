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
#include "target_device_table_i.h"

static void test_usbi3c_bus_devices(void **state)
{
	struct usbi3c_device *usbi3c = NULL;
	uint8_t *cap_buffer = NULL;
	uint8_t *target_dev_table_buffer = NULL;
	uint8_t *target_dev_table_buffer2 = NULL;
	int res = 0;
	int fake_pointer = 1;

	const int NUMBER_OF_DEVICES = 3;
	const int DEVICE_CAPABILITY = 0b000000000111;
	const int TARGET_CONFIG = 0b00000000;
	const int TARGET_CAPABILITY = 0b1000000000;
	const int I3C_DEVICE_TYPE = 0;

	// Now change the timeout and check that it was applied
	const long int usb_timeout = 2000;

	usbi3c = helper_usbi3c_init(&fake_pointer);

	usbi3c_set_timeout(usbi3c, usb_timeout);
	test_timeout = usb_timeout;

	// create capability buffer
	cap_buffer = mock_get_i3c_capability(&fake_pointer, DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA, DEVICE_CAPABILITY, NUMBER_OF_DEVICES, RETURN_SUCCESS);

	// initialize_i3c_bus mocking
	mock_initialize_i3c_bus(&fake_pointer, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);

	// create target device table buffer
	target_dev_table_buffer = mock_get_target_device_table(&fake_pointer, NUMBER_OF_DEVICES, TARGET_CONFIG, TARGET_CAPABILITY, RETURN_SUCCESS);

	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(&fake_pointer,
												INITIAL_TARGET_ADDRESS_POOL,
												NUMBER_OF_DEVICES,
												DEVICE_CAPABILITY,
												DEFAULT_MAX_IBI_PAYLOAD_SIZE,
												RETURN_SUCCESS);

	// create target device table buffer
	target_dev_table_buffer2 = mock_get_target_device_table(&fake_pointer,
								NUMBER_OF_DEVICES,
								TARGET_CONFIG,
								TARGET_CAPABILITY,
								RETURN_SUCCESS);

	// fake interrupt
	int interrupt_endpoint = 3;
	struct notification_format *notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;
	fake_transfer_add_data(interrupt_endpoint, (unsigned char *)notification, sizeof(struct notification_format));
	mock_libusb_wait_for_events_trigger(interrupt_endpoint, 0);
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);

	res = usbi3c_initialize_device(usbi3c);

	assert_int_equal(res, 0);
	for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
		assert_non_null(table_get_device(usbi3c->target_device_table, INITIAL_TARGET_ADDRESS_POOL + i));
	}
	uint8_t *address_list = NULL;
	res = usbi3c_get_address_list(usbi3c, &address_list);
	assert_int_equal(res, NUMBER_OF_DEVICES);

	for (int i = 0; i < res; i++) {
		assert_int_equal(usbi3c_get_target_BCR(usbi3c, address_list[i]), INITIAL_BCR_POOL + i);
		assert_int_equal(usbi3c_get_target_DCR(usbi3c, address_list[i]), INITIAL_DCR_POOL + i);
		assert_int_equal(usbi3c_get_target_type(usbi3c, address_list[i]), I3C_DEVICE_TYPE);
	}

	free(notification);
	free(address_list);
	free(configure_buffer);
	free(cap_buffer);
	free(target_dev_table_buffer);
	free(target_dev_table_buffer2);

	helper_usbi3c_deinit(&usbi3c, &fake_pointer);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_usbi3c_bus_devices)
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
