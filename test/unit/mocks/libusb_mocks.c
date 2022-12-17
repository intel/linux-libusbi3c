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

#include "mocks.h"

int default_fake_device = 1;
int default_fake_handle = 1;
int default_fake_usb_context = 1;
int submit_transfer_fail = 0;

void mock_libusb_init(void *fake_usb_context, int return_code)
{
	if (fake_usb_context == NULL) {
		fake_usb_context = &default_fake_usb_context;
	}
	will_return(__wrap_libusb_init, &fake_usb_context);
	will_return(__wrap_libusb_init, return_code); /* 0 on success */
}

void mock_libusb_open(void *fake_handle, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	will_return(__wrap_libusb_open, fake_handle);
	will_return(__wrap_libusb_open, return_code); /* 0 on success */
}

void mock_libusb_close(void)
{
	will_return(__wrap_libusb_close, NULL);
}

void mock_libusb_claim_interface(void *fake_handle, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	expect_value(__wrap_libusb_claim_interface, dev_handle, fake_handle);
	expect_value(__wrap_libusb_claim_interface, interface_number, 0);
	will_return(__wrap_libusb_claim_interface, return_code); /* 0 on success */
}

void mock_libusb_release_interface(void *fake_handle, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	expect_value(__wrap_libusb_release_interface, dev_handle, fake_handle);
	expect_value(__wrap_libusb_release_interface, interface_number, 0);
	will_return(__wrap_libusb_release_interface, return_code); /* 0 on success */
}

void mock_libusb_kernel_driver_active(void *fake_handle, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	expect_value(__wrap_libusb_kernel_driver_active, dev_handle, fake_handle);
	expect_value(__wrap_libusb_kernel_driver_active, interface_number, 0);
	will_return(__wrap_libusb_kernel_driver_active, return_code); /* 0 if no kernel driver is active, 1 otherwise */
}

void mock_libusb_detach_kernel_driver(void *fake_handle, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	expect_value(__wrap_libusb_detach_kernel_driver, dev_handle, fake_handle);
	expect_value(__wrap_libusb_detach_kernel_driver, interface_number, 0);
	will_return(__wrap_libusb_detach_kernel_driver, return_code);
}

void mock_libusb_wait_for_events_trigger(int endpoint, int return_code)
{
	will_return(__wrap_libusb_wait_for_event, 1);
	will_return(__wrap_libusb_wait_for_event, endpoint);
	will_return(__wrap_libusb_wait_for_event, return_code);
}

void mock_libusb_wait_for_events_not_trigger(int endpoint, int return_code)
{
	will_return_always(__wrap_libusb_wait_for_event, 0);
	will_return_always(__wrap_libusb_wait_for_event, endpoint);
	will_return_always(__wrap_libusb_wait_for_event, return_code);
}

void mock_libusb_control_transfer(void *fake_handle, struct control_transfer_mock *request, int return_code)
{
	if (fake_handle == NULL) {
		fake_handle = &default_fake_handle;
	}
	expect_value(__wrap_libusb_control_transfer, dev_handle, fake_handle);
	expect_value(__wrap_libusb_control_transfer, bmRequestType, request->bmRequestType);
	expect_value(__wrap_libusb_control_transfer, bRequest, request->bRequest);
	expect_value(__wrap_libusb_control_transfer, wValue, request->wValue);
	expect_value(__wrap_libusb_control_transfer, wIndex, request->wIndex);
	expect_value(__wrap_libusb_control_transfer, wLength, request->wLength);
	expect_value(__wrap_libusb_control_transfer, timeout, request->timeout);
	if (request->bmRequestType & INPUT_TRANSFER_REQUEST) {
		will_return(__wrap_libusb_control_transfer, request->data);
	} else if (request->wLength > 0) {
		expect_memory(__wrap_libusb_control_transfer, data, request->data, request->wLength);
	}

	will_return(__wrap_libusb_control_transfer, return_code);
}

void mock_libusb_get_device(int force_failure)
{
	if (force_failure != 0) {
		will_return(__wrap_libusb_get_device, NULL);
	} else {
		will_return(__wrap_libusb_get_device, &default_fake_device);
	}
}

void mock_libusb_get_max_packet_size(int force_failure)
{
	if (force_failure != 0) {
		will_return(__wrap_libusb_get_max_packet_size, LIBUSB_ERROR_NOT_FOUND);
	} else {
		will_return(__wrap_libusb_get_max_packet_size, 32);
	}
}

void mock_libusb_alloc_transfer(void *fake_transfer)
{
	will_return(__wrap_libusb_alloc_transfer, fake_transfer);
}

void mock_libusb_free_transfer(void)
{
	/* Intentionally left empty */
}

void mock_libusb_get_device_descriptor(int device_class, int product, int vendor, int return_code)
{
	will_return(__wrap_libusb_get_device_descriptor, device_class);
	will_return(__wrap_libusb_get_device_descriptor, product);
	will_return(__wrap_libusb_get_device_descriptor, vendor);
	will_return(__wrap_libusb_get_device_descriptor, return_code);
}

void mock_libusb_get_device_list(struct libusb_device **device_list, int return_code)
{
	will_return(__wrap_libusb_get_device_list, device_list);
	will_return(__wrap_libusb_get_device_list, return_code);
}

void mock_libusb_free_device_list(struct libusb_device **usb_devices, int unref_devices)
{
	expect_value(__wrap_libusb_free_device_list, list, usb_devices);
	expect_value(__wrap_libusb_free_device_list, unref_devices, unref_devices);
}

void mock_libusb_ref_device(struct libusb_device *device_to_ref)
{
	expect_value(__wrap_libusb_ref_device, device, device_to_ref);
}

void mock_libusb_submit_transfer(int return_code)
{
	submit_transfer_fail = return_code;
}

void mock_libusb_bulk_transfer(unsigned char *buffer, int buffer_size, int endpoint, int return_code)
{
	/* validate that the function gets the correct endpoint for the bulk transfers */
	expect_value(__wrap_libusb_bulk_transfer, endpoint, endpoint);
	/* validate that the buffer to be transferred matches what we expect */
	expect_memory(__wrap_libusb_bulk_transfer, data, buffer, buffer_size);

	/* instead of attempting an actual USB bulk transfer, we will return these
	 * values when the libusb_bulk_transfer() function is called */
	will_return(__wrap_libusb_bulk_transfer, buffer_size);
	will_return(__wrap_libusb_bulk_transfer, return_code);
}
