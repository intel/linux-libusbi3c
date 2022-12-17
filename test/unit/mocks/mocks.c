
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
#include "helpers.h"

/* these mocks are disabled by default, these flags are used to enable a mock */
int enable_mock_libusb_alloc_transfer = FALSE;
int enable_mock_libusb_free_transfer = FALSE;

int __wrap_libusb_init(struct libusb_context **ctx)
{
	*ctx = mock_ptr_type(struct libusb_context *);
	fake_transfer_init();
	return mock_type(int);
}

void __wrap_libusb_exit(struct libusb_context *ctx)
{
	fake_transfer_deinit();
}

int __wrap_libusb_get_device_list(struct libusb_context *ctx, struct libusb_device ***list)
{
	*list = mock_ptr_type(struct libusb_device **);
	return mock_type(int);
}

void __wrap_libusb_free_device_list(libusb_device **list, int unref_devices)
{
	check_expected_ptr(list);
	check_expected(unref_devices);
}

void __wrap_libusb_ref_device(struct libusb_device *device)
{
	check_expected(device);
}

int __wrap_libusb_get_device_descriptor(struct libusb_device *device, struct libusb_device_descriptor *desc)
{
	desc->bDeviceClass = mock_type(int);
	desc->idProduct = mock_type(int);
	desc->idVendor = mock_type(int);
	return mock_type(int);
}

int __wrap_libusb_get_string_descriptor_ascii(struct libusb_device_handle *handle, uint8_t desc_index, unsigned char *data, int length)
{
	data = mock_ptr_type(unsigned char *);
	return mock_type(int);
}

int __wrap_libusb_open(struct libusb_device *dev, struct libusb_device_handle **handle)
{
	*handle = mock_ptr_type(struct libusb_device_handle *);
	return mock_type(int);
}

void __wrap_libusb_close(struct libusb_device_handle *handle)
{
	handle = mock_ptr_type(struct libusb_device_handle *);
}

int __wrap_libusb_kernel_driver_active(libusb_device_handle *dev_handle, int interface_number)
{
	check_expected_ptr(dev_handle);
	check_expected(interface_number);
	return mock_type(int);
}

int __wrap_libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
	check_expected_ptr(dev_handle);
	check_expected(interface_number);
	return mock_type(int);
}

int __wrap_libusb_claim_interface(libusb_device_handle *dev_handle, int interface_number)
{
	check_expected_ptr(dev_handle);
	check_expected(interface_number);
	return mock_type(int);
}

int __wrap_libusb_release_interface(struct libusb_device_handle *dev_handle, int interface_number)
{
	check_expected_ptr(dev_handle);
	check_expected(interface_number);
	return mock_type(int);
}

int __wrap_libusb_control_transfer(libusb_device_handle *dev_handle,
				   uint8_t bmRequestType,
				   uint8_t bRequest,
				   uint16_t wValue,
				   uint16_t wIndex,
				   unsigned char *data,
				   uint16_t wLength,
				   unsigned int timeout)
{
	check_expected_ptr(dev_handle);
	check_expected(bmRequestType);
	check_expected(bRequest);
	check_expected(wValue);
	check_expected(wIndex);
	if (bmRequestType & INPUT_TRANSFER_REQUEST) {
		unsigned char *data_param = mock_ptr_type(unsigned char *);
		memcpy(data, data_param, wLength);
	} else if (wLength > 0) {
		check_expected_ptr(data);
	}
	check_expected(wLength);
	check_expected(timeout);
	return mock_type(int);
}

int __wrap_libusb_bulk_transfer(libusb_device_handle *dev_handle,
				unsigned char endpoint,
				unsigned char *data,
				int length,
				int *actual_length,
				unsigned int timeout)
{
	unsigned char *mock_data = NULL;
	check_expected(endpoint);

	if (endpoint & INPUT_TRANSFER_REQUEST) {
		/* this is an input transfer - we need to build a
		 * test data for the bulk response transfer */
		mock_data = mock_ptr_type(unsigned char *);
		if (mock_data) {
			memcpy(data, mock_data, length);
		} else {
			data = NULL;
		}
	} else {
		/* this is an output transfer - let's make sure the data the
		 * libusb_bulk_transfer() is supposed to send is formatted correctly */
		check_expected_ptr(data);
	}

	*actual_length = mock_type(int);

	return mock_type(int);
}

struct libusb_device *__wrap_libusb_get_device(struct libusb_device_handle *dev_handle)
{
	return mock_ptr_type(struct libusb_device *);
}

int __wrap_libusb_get_max_packet_size(struct libusb_device *device, unsigned char endpoint)
{
	return mock_type(int);
}

int __wrap_libusb_submit_transfer(struct libusb_transfer *transfer)
{
	extern int submit_transfer_fail;
	if (submit_transfer_fail > 0) {
		/* this will force libusb_submit_transfer to succeed a
		 * few times then if we keep calling the function it will
		 * eventually is going to fail */
		submit_transfer_fail--;
		if (submit_transfer_fail == 0) {
			submit_transfer_fail = -1;
		}
	} else if (submit_transfer_fail < 0) {
		int to_return = submit_transfer_fail;
		submit_transfer_fail = 0;
		return to_return;
	}
	int endpoint = transfer->endpoint & USB_ENDPOINT_MASK;
	if (fake_transfer_get_transfer(endpoint) == NULL) {
		fake_transfer_add_transfer(endpoint, transfer);
	}
	return 0;
}

void __wrap_libusb_lock_event_waiters(struct libusb_context *ctx)
{
	/* Intentionally left empty */
}

void __wrap_libusb_unlock_event_waiters(struct libusb_context *ctx)
{
	/* Intentionally left empty */
}

int __wrap_libusb_wait_for_event(struct libusb_context *ctx, struct timeval *tv)
{
	int trigger = mock_type(int);
	int endpoint = mock_type(int);
	if (trigger) {
		fake_transfer_trigger(endpoint);
	}
	return mock_type(int);
}

int __wrap_libusb_handle_events(struct libusb_context *ctx)
{
	fake_transfer_emit();
	return 0;
}

void __wrap_libusb_unref_device(struct libusb_device *dev)
{
}

struct libusb_transfer *__real_libusb_alloc_transfer(int iso_packets);
struct libusb_transfer *__wrap_libusb_alloc_transfer(int iso_packets)
{
	if (enable_mock_libusb_alloc_transfer == FALSE) {
		return __real_libusb_alloc_transfer(iso_packets);
	}

	return mock_ptr_type(struct libusb_transfer *);
}

void __real_libusb_free_transfer(struct libusb_transfer *transfer);
void __wrap_libusb_free_transfer(struct libusb_transfer *transfer)
{
	if (enable_mock_libusb_free_transfer == FALSE) {
		return __real_libusb_free_transfer(transfer);
	}
}

int __wrap_libusb_wrap_sys_device(libusb_context *ctx, intptr_t sys_dev, libusb_device_handle **dev_handle)
{
	*dev_handle = mock_ptr_type(struct libusb_device_handle *);
	return mock_type(int);
}
