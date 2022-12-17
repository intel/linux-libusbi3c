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

void mock_usb_bulk_transfer_response_buffer_init(int return_code)
{
	mock_usb_get_max_bulk_response_buffer_size(return_code);
}

void mock_usb_get_max_bulk_response_buffer_size(int return_code)
{
	mock_libusb_get_max_packet_size(return_code);
}

void mock_usb_input_bulk_transfer_polling(int return_code)
{
	mock_libusb_submit_transfer(return_code);
}

void mock_usb_output_bulk_transfer(unsigned char *buffer, int buffer_size, int return_code)
{
	const int BULK_TRANSFER_OUT = 0x2;
	mock_libusb_bulk_transfer(buffer, buffer_size, BULK_TRANSFER_OUT, return_code);
}

void mock_usb_wait_for_next_event(int endpoint, unsigned char *buffer, int buffer_size, int return_code)
{
	fake_transfer_add_data(endpoint, buffer, buffer_size);
	/* this next line mocks libusb_wait_for_event() */
	mock_libusb_wait_for_events_trigger(endpoint, return_code);
}

void mock_usb_init(void *usb_context, int return_code)
{
	mock_libusb_init(usb_context, return_code);
}

void mock_usb_deinit(void *handle, int return_code)
{
	mock_libusb_release_interface(handle, return_code);
	mock_libusb_close();
}

void mock_usb_set_device(void *handle, int return_code)
{
	mock_libusb_open(handle, return_code);
}

void mock_usb_interrupt_init(int return_code)
{
	mock_libusb_submit_transfer(return_code);
}

void mock_usb_set_interface(void *handle, int return_code)
{
	mock_libusb_kernel_driver_active(handle, return_code);
	mock_libusb_claim_interface(handle, return_code);
}

void mock_usb_set_device_for_io(void *handle, int return_code)
{
	mock_usb_set_device(handle, return_code);
	mock_usb_set_interface(handle, return_code);
}

void mock_usb_input_control_transfer_async(int return_code)
{
	mock_libusb_submit_transfer(return_code);
}

void mock_usb_output_control_transfer_async(int return_code)
{
	mock_libusb_submit_transfer(return_code);
}
