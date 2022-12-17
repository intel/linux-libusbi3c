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

#ifndef __unit_test_mocks_h__
#define __unit_test_mocks_h__

#include <libusb.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

#include "usbi3c_i.h"

/* USB endpoint index */
#define USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX 0x00
#define USBI3C_BULK_TRANSFER_ENDPOINT_INDEX 0x02
#define USBI3C_INTERRUPT_ENDPOINT_INDEX 0x03

/* USB endpoint direction */
#define OUTPUT_TRANSFER_REQUEST 0x00
#define INPUT_TRANSFER_REQUEST 0x80

/* Variables used to generate fake target devices connected to the I3C bus.
 * If multiple devices are needed in the bus, these are used as starting
 * points and each new device will use the next incremental value to make
 * tests deterministic.
 * For example 3 target devices in a bus would use these addresses:
 * Address Device 1 = INITIAL_TARGET_ADDRESS_POOL
 * Address Device 2 = INITIAL_TARGET_ADDRESS_POOL + 1
 * Address Device 3 = INITIAL_TARGET_ADDRESS_POOL + 2 */
#define INITIAL_TARGET_ADDRESS_POOL 100
#define INITIAL_PID_LO_POOL 0xFF00
#define INITIAL_DCR_POOL 10
#define INITIAL_BCR_POOL 20
#define INITIAL_IBI_PRIORITIZATION_POOL 0x01
#define DEFAULT_MAX_IBI_PAYLOAD_SIZE 0x400
#define USBI3C_DEVICE_STATIC_ADDRESS 0x32
#define MIPI_MAJOR_VER 0x1
#define MIPI_MINOR_VER 0x2

/* used when building capability buffer */
#define DEFAULT_TARGET_CAPABILITY 0x00
#define DEFAULT_TARGET_CONFIGURATION 0x00
#define DEFAULT_CONTROLLER_CAPABILITY 0b000000000111
#define NO_DEVICES_IN_BUS 0

/* holds the control transfer timeout used in tests */
extern int test_timeout;

struct control_transfer_mock {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
	unsigned char *data;
	unsigned int timeout;
};

/* libusb_mocks.c */
void mock_libusb_init(void *fake_usb_context, int return_code);
void mock_libusb_open(void *fake_handle, int return_code);
void mock_libusb_close(void);
void mock_libusb_claim_interface(void *fake_handle, int return_code);
void mock_libusb_release_interface(void *fake_handle, int return_code);
void mock_libusb_kernel_driver_active(void *fake_handle, int return_code);
void mock_libusb_detach_kernel_driver(void *fake_handle, int return_code);
void mock_libusb_wait_for_events_trigger(int endpoint, int return_code);
void mock_libusb_wait_for_events_not_trigger(int endpoint, int return_code);
void mock_libusb_control_transfer(void *fake_handle, struct control_transfer_mock *request, int return_code);
void mock_libusb_get_max_packet_size(int force_failure);
void mock_libusb_get_device(int force_failure);
void mock_libusb_submit_transfer(int return_code);
void mock_libusb_alloc_transfer(void *fake_transfer);
void mock_libusb_free_transfer(void);
void mock_libusb_get_device_descriptor(int device_class, int product, int vendor, int return_code);
void mock_libusb_get_device_list(struct libusb_device **device_list, int return_code);
void mock_libusb_free_device_list(struct libusb_device **usb_devices, int unref_devices);
void mock_libusb_ref_device(struct libusb_device *device_to_ref);
void mock_libusb_bulk_transfer(unsigned char *buffer, int buffer_size, int endpoint, int return_code);

/* usb_mocks.c */
void mock_usb_bulk_transfer_response_buffer_init(int return_code);
void mock_usb_get_max_bulk_response_buffer_size(int return_code);
void mock_usb_input_bulk_transfer_polling(int return_code);
void mock_usb_output_bulk_transfer(unsigned char *buffer, int buffer_size, int return_code);
void mock_usb_wait_for_next_event(int endpoint, unsigned char *buffer, int buffer_size, int return_code);
void mock_usb_init(void *usb_context, int return_code);
void mock_usb_deinit(void *handle, int return_code);
void mock_usb_set_device(void *handle, int return_code);
void mock_usb_interrupt_init(int return_code);
void mock_usb_set_interface(void *handle, int return_code);
void mock_usb_set_device_for_io(void *handle, int return_code);
void mock_usb_input_control_transfer_async(int return_code);
void mock_usb_output_control_transfer_async(int return_code);

/* class_specific_request_mocks.c */
unsigned char *create_i3c_capability_buffer(int error_code, int device_role, int data_type, int address, int capability, int num_entries);
unsigned char *create_target_device_table_buffer(int num_entries, int config, int capability);
void mock_clear_feature(int feature_selector, int return_code);
void mock_set_feature(int feature_selector, int return_code);
unsigned char *mock_get_i3c_capability(void *fake_handle, int error_code, int device_role, int data_type, int capability, int num_entries, int return_code);
void mock_initialize_i3c_bus(void *fake_handle, int mode, int return_code);
unsigned char *mock_get_target_device_table(void *fake_handle, int num_entries, int target_config, int target_capability, int return_code);
unsigned char *mock_set_target_device_config(uint8_t address, uint8_t ibit, uint8_t crr, uint8_t tir, int return_code);
unsigned char *mock_set_target_max_ibi_payload(uint8_t address, uint32_t payload_size, int return_code);
unsigned char *mock_set_target_device_configs_from_device_capability(void *fake_handle, uint8_t address_start, int num_entries, int dev_capability, uint32_t payload_size, int return_code);
unsigned char *mock_change_dynamic_address(uint8_t old_address, uint8_t new_address, int return_code);
unsigned char *mock_get_address_change_result(uint8_t old_address, uint8_t new_address, int address_change_status, int return_code);
void mock_get_buffer_available(void *fake_handle, int *buffer_available, int return_code);
void mock_cancel_or_resume_bulk_request(int return_code);

#endif //__unit_test_mocks_h__
