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
#include "target_device_table_i.h"

#include "usbi3c.h"

struct libusb_device {
	int fake_device_member;
};

struct usbi3c_device *helper_usbi3c_init(void *handle)
{
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **usbi3c_devices = NULL;
	struct usbi3c_device *usbi3c_dev = NULL;
	int ret = -1;

	mock_usb_init(NULL, RETURN_SUCCESS);
	ctx = usbi3c_init();
	assert_non_null(ctx);

	struct libusb_device libusb_device = { .fake_device_member = 1 };
	struct libusb_device *usb_devices[2];
	usb_devices[0] = &libusb_device;
	usb_devices[1] = NULL;

	mock_libusb_get_device_list(usb_devices, 1);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&libusb_device);
	mock_libusb_free_device_list(usb_devices, TRUE);
	mock_libusb_open(handle, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(handle, 0);
	mock_libusb_claim_interface(handle, 0);

	ret = usbi3c_get_devices(ctx, 100, 2, &usbi3c_devices);
	assert_int_equal(ret, 1);
	assert_non_null(usbi3c_devices);
	usbi3c_dev = usbi3c_ref_device(usbi3c_devices[0]);

	usbi3c_deinit(&ctx);
	usbi3c_free_devices(&usbi3c_devices);

	return usbi3c_dev;
}

void helper_usbi3c_deinit(struct usbi3c_device **usbi3c_dev, void *handle)
{
	/* mock libusb functions */
	mock_usb_deinit(handle, RETURN_SUCCESS);

	usbi3c_device_deinit(usbi3c_dev);
}

struct list *helper_create_test_list(int a, int b)
{
	struct list *head = NULL;
	struct list *node = NULL;
	int *value = NULL;

	head = (struct list *)calloc(1, sizeof(struct list));
	node = head;

	for (int i = a; i <= b; i++) {
		value = (int *)calloc(1, sizeof(int));
		*value = i;
		node->data = value;
		if (i < b) {
			node->next = (struct list *)calloc(1, sizeof(struct list));
			node = node->next;
		}
	}

	return head;
}

void helper_create_dummy_devices_in_target_device_table(struct usbi3c_device *usbi3c_dev, int number_of_devices)
{
	uint8_t *target_dev_table_buffer = NULL;
	uint16_t buffer_size = TARGET_DEVICE_HEADER_SIZE + (TARGET_DEVICE_ENTRY_SIZE * number_of_devices);

	/* create a buffer with N number of devices in the device table */
	target_dev_table_buffer = create_target_device_table_buffer(number_of_devices, DEFAULT_TARGET_CONFIGURATION, DEFAULT_TARGET_CAPABILITY);
	table_fill_from_device_table_buffer(usbi3c_dev->target_device_table, target_dev_table_buffer, buffer_size);

	free(target_dev_table_buffer);
}

int helper_initialize_controller(struct usbi3c_device *usbi3c, void *handle, uint8_t **address_list)
{
	int res = 0;
	int address_size = 0;
	uint8_t *cap_buffer = NULL;
	uint8_t *target_dev_table_buffer = NULL;
	uint8_t *target_dev_table_buffer2 = NULL;
	struct notification_format *notification = NULL;
	const int DEVICES_IN_BUS = 3;

	/* dummy notification that is going to be "received" for
	 * successful bus initialization */
	notification = malloc(sizeof(struct notification_format));
	notification->type = NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS;
	notification->code = SUCCESSFUL_I3C_BUS_INITIALIZATION;

	/* mock the libusb functions that are to be called during initialization */
	cap_buffer = mock_get_i3c_capability(handle,
					     DEVICE_CONTAINS_CAPABILITY_DATA,
					     USBI3C_PRIMARY_CONTROLLER_ROLE,
					     STATIC_DATA,
					     DEFAULT_CONTROLLER_CAPABILITY,
					     DEVICES_IN_BUS,
					     RETURN_SUCCESS);
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);
	mock_usb_input_bulk_transfer_polling(RETURN_SUCCESS);
	mock_usb_interrupt_init(RETURN_SUCCESS);
	mock_initialize_i3c_bus(handle, I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT, RETURN_SUCCESS);
	mock_usb_wait_for_next_event(USBI3C_INTERRUPT_ENDPOINT_INDEX,
				     (unsigned char *)notification,
				     sizeof(struct notification_format),
				     RETURN_SUCCESS);
	target_dev_table_buffer = mock_get_target_device_table(handle,
							       DEVICES_IN_BUS,
							       DEFAULT_TARGET_CONFIGURATION,
							       DEFAULT_TARGET_CAPABILITY,
							       RETURN_SUCCESS);
	// set configuration
	unsigned char *configure_buffer = mock_set_target_device_configs_from_device_capability(handle,
												INITIAL_TARGET_ADDRESS_POOL,
												DEVICES_IN_BUS,
												DEFAULT_CONTROLLER_CAPABILITY,
												DEFAULT_MAX_IBI_PAYLOAD_SIZE,
												RETURN_SUCCESS);

	// updating target device table with new config
	target_dev_table_buffer2 = mock_get_target_device_table(handle,
								DEVICES_IN_BUS,
								DEFAULT_TARGET_CONFIGURATION,
								DEFAULT_TARGET_CAPABILITY,
								RETURN_SUCCESS);

	res = usbi3c_initialize_device(usbi3c);
	assert_int_equal(res, RETURN_SUCCESS);

	if (address_list != NULL) {
		address_size = usbi3c_get_address_list(usbi3c, address_list);
	}

	free(notification);
	free(configure_buffer);
	free(cap_buffer);
	free(target_dev_table_buffer);
	free(target_dev_table_buffer2);

	return address_size;
}

void helper_initialize_target_device(struct usbi3c_device *usbi3c, void *handle)
{
	uint8_t *cap_buffer = NULL;
	unsigned char *request_buffer = NULL;
	unsigned char *response_buffer = NULL;
	int request_buffer_size = 0;
	int response_buffer_size = 0;
	int buffer_available = 1000;
	int ret = -1;
	const int DEVICES_IN_BUS = 0;

	/* mock the libusb functions that are to be called during initialization */
	cap_buffer = mock_get_i3c_capability(handle,
					     DEVICE_CONTAINS_CAPABILITY_DATA,
					     USBI3C_TARGET_DEVICE_ROLE,
					     NO_STATIC_DATA,
					     DEFAULT_TARGET_CAPABILITY,
					     DEVICES_IN_BUS,
					     RETURN_SUCCESS);
	mock_usb_bulk_transfer_response_buffer_init(RETURN_SUCCESS);
	mock_usb_input_bulk_transfer_polling(RETURN_SUCCESS);
	mock_get_buffer_available(handle, &buffer_available, RETURN_SUCCESS);

	/* when initializing a target device we send a bulk request transfer
	 * with a hot-join request to the active controller.
	 * let's create the type of buffer we expect so we can compare it against
	 * the one generated by the library */
	request_buffer_size = helper_create_command_buffer(bulk_request_id,
							   &request_buffer,
							   HOT_JOIN_ADDRESS,
							   USBI3C_WRITE,
							   USBI3C_TERMINATE_ON_ANY_ERROR,
							   USBI3C_RESPONSE_HAS_NO_DATA,
							   NULL,
							   DEFAULT_TRANSFER_MODE,
							   DEFAULT_TRANSFER_RATE,
							   USBI3C_NOT_DEPENDENT_ON_PREVIOUS);
	mock_usb_output_bulk_transfer(request_buffer, request_buffer_size, RETURN_SUCCESS);
	/* the bulk request generates a bulk response that shows if the request was delivered
	 * successfully or not. Let's create the type of buffer we expect to receive so we mock
	 * a response from the I3C controller device */
	struct usbi3c_response response;
	response.attempted = USBI3C_COMMAND_ATTEMPTED;
	response.error_status = USBI3C_SUCCEEDED;
	response.has_data = USBI3C_RESPONSE_HAS_NO_DATA;
	response.data_length = 0;
	response.data = NULL;
	response_buffer_size = helper_create_response_buffer(&response_buffer,
							     &response,
							     bulk_request_id);
	mock_usb_wait_for_next_event(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response_buffer, response_buffer_size, RETURN_SUCCESS);

	ret = usbi3c_initialize_device(usbi3c);
	assert_int_equal(ret, RETURN_SUCCESS);

	free(cap_buffer);
	free(request_buffer);
	free(response_buffer);
}

void helper_trigger_incoming_transfer(int endpoint, unsigned char *buffer, int buffer_size)
{
	fake_transfer_add_data(endpoint, buffer, buffer_size);
	fake_transfer_trigger(endpoint);
}

void helper_trigger_notification(unsigned char *notification_buffer, int buffer_size)
{
	helper_trigger_incoming_transfer(USBI3C_INTERRUPT_ENDPOINT_INDEX, notification_buffer, buffer_size);
}

void helper_trigger_response(unsigned char *response_buffer, int buffer_size)
{
	helper_trigger_incoming_transfer(USBI3C_BULK_TRANSFER_ENDPOINT_INDEX, response_buffer, buffer_size);
}

void helper_trigger_control_transfer(unsigned char *control_transfer_buffer, int buffer_size)
{
	helper_trigger_incoming_transfer(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, control_transfer_buffer, buffer_size);
}

size_t helper_create_notification_buffer(uint8_t **buffer, int type, int code)
{
	struct notification_format *nf = calloc(1, sizeof(struct notification_format));
	if (nf == NULL) {
		return -1;
	}
	nf->type = type;
	nf->code = code;
	*buffer = (uint8_t *)nf;
	return sizeof(*nf);
}

int helper_create_address_change_result_buffer(unsigned char **buffer, uint8_t current_address, uint8_t new_address, int num_entries, int status)
{
	uint32_t *ptr_32 = NULL;
	int buffer_size = 0;
	int size = 0;

	/* NOTE: num_entries should be 1 unless you want to force the operation to fail */

	/* an address change result data structure is 2 DW long:
	 * Header (1 DW) + Entry body (1 DW) */
	size = (1 * DWORD_SIZE) + (1 * DWORD_SIZE);

	/* currently we only support changing the address of one I3C device at a time,
	 * so we can assume the response will only contain one address change result.
	 * An address change result data structure for changing the address of 1 entry
	 * is 2 DW long: Header (1 DW) + Address Change Response body (1 DW) */
	buffer_size = 2 * DWORD_SIZE;
	*buffer = (unsigned char *)calloc(1, buffer_size);

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)*buffer;

	/* 31:16 Reserved, 15:8 NumEntries, 7:0 Size */
	*(ptr_32 += 0) = (num_entries << 8) + size;
	/* 31:17 Reserved, 16 S/F, 15:8 New Address, 7:0 Current Address */
	*(ptr_32 += 1) = (status << 16) + (new_address << 8) + current_address;

	return buffer_size;
}
