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

/* holds the control transfer timeout used in tests */
int test_timeout = 1000;

#define SINGLE_ADDRESS 1

/*******************************
 * Functions for buffer creation
 *******************************/

unsigned char *create_i3c_capability_buffer(int error_code, int device_role, int data_type, int address, int capability, int num_entries)
{
	uint32_t *ptr_32 = NULL;
	uint16_t length = 0;
	uint8_t target_static_address = INITIAL_TARGET_ADDRESS_POOL;
	int pid_lo = INITIAL_PID_LO_POOL;
	int pid_hi = 0;
	int ibi_prioritization = INITIAL_IBI_PRIORITIZATION_POOL;

	if (error_code == DEVICE_CONTAINS_CAPABILITY_DATA) {
		/* an I3C Capability data structure is at least 14 DW long:
		 * Header (1 DW) + I3C Device Data (9 DW) + Target Device N Config Data (4 DW) */
		length = (1 * DWORD_SIZE) + (9 * DWORD_SIZE) + (4 * DWORD_SIZE * num_entries);
	} else {
		/* When I3C Device does not contain I3C Capability data, I3C Function should
		 * return only the I3C_CAPABILITY_HEADER of I3C Capability data structure with
		 * Total Length set to 4 bytes, Device Role and Data Type set to 0h and the Error Code field set to FFh */
		length = (1 * DWORD_SIZE);
	}

	unsigned char *buffer = (unsigned char *)calloc(1, USB_MAX_CONTROL_BUFFER_SIZE);
	ON_NULL_ABORT(buffer);

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)buffer;

	/*** Header ***/
	/* 31:24 Error Code, 23:30 Reserved, 19:18 Data type, 17:16 Device role, 15:0 Length */
	*(ptr_32) = (error_code << 24) + (data_type << 18) + (device_role << 16) + length;
	if (error_code == DEVICE_DOES_NOT_CONTAIN_CAPABILITY_DATA) {
		return buffer;
	}

	/*** I3C Device Data ***/
	/* 31:20 Capability Flags, 19:16 Reserved, 15:8 I2C Devices Present, 7:0 Static Address */
	*(ptr_32 + 1) = (capability << 20) + address;
	/* 31:16 Major version number, 15:0 Minor version number */
	*(ptr_32 + 2) = (MIPI_MAJOR_VER << 16) + MIPI_MINOR_VER;
	*(ptr_32 + 3) = (MIPI_MAJOR_VER << 16) + MIPI_MINOR_VER;
	*(ptr_32 + 4) = 0x00000000;
	*(ptr_32 + 5) = 0x00000000;
	/* 31:16 TMEC length, 15:8 I3C DTR, 7:0 I3C DTM */
	*(ptr_32 + 6) = (0b00001111 << 8) + 0b00000011;
	*(ptr_32 + 7) = 0x00000000;
	*(ptr_32 + 8) = 0x00000001;
	*(ptr_32 + 9) = DEFAULT_MAX_IBI_PAYLOAD_SIZE;

	/*** Target Device N Configuration Data ***/
	ptr_32 += 9;
	for (int i = 0; i < num_entries; i++) {
		/* 31:16 PID_LO, 15:8 IBI Prioritization, 7:0 Static Address */
		*(ptr_32 += 1) = (pid_lo << 16) + (ibi_prioritization << 8) + target_static_address;
		*(ptr_32 += 1) = pid_hi;
		/* 31:16 Major version number, 15:0 Minor version number */
		*(ptr_32 += 1) = (MIPI_MAJOR_VER << 16) + MIPI_MINOR_VER;
		*(ptr_32 += 1) = 0x00000000;

		target_static_address++;
		ibi_prioritization++;
		pid_lo++;
	}

	return buffer;
}

unsigned char *create_target_device_table_buffer(int num_entries, int config, int capability)
{
	uint32_t *ptr_32 = NULL;
	uint16_t size = 0;
	/* devices in the table start with these values and increase by 1
	 * for each new device */
	uint8_t target_address = INITIAL_TARGET_ADDRESS_POOL;
	uint8_t dcr = INITIAL_DCR_POOL;
	uint8_t bcr = INITIAL_BCR_POOL;
	uint16_t pid_lo = INITIAL_PID_LO_POOL;
	uint32_t pid_hi = 0;

	unsigned char *buffer = (unsigned char *)calloc(1, USB_MAX_CONTROL_BUFFER_SIZE);
	ON_NULL_ABORT(buffer);

	/* a Target Device Table data structure is at least 5 DW long:
	 * Header (1 DW) + Target Device N Entry (4 DW) */
	size = (1 * DWORD_SIZE) + (4 * DWORD_SIZE * num_entries);

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)buffer;

	/*** Header ***/
	/* 31:16 Reserved, 15:0 Table Size */
	*(ptr_32) = size;
	/*** Target Device N Entry ***/
	for (int i = 0; i < num_entries; i++) {
		/* 31:26 Reserved, 25:16 Target Capability, 15:14 Reserved, 13:8 configurable values , 7:0 Address*/
		*(ptr_32 += 1) = (capability << 16) + (config << 8) + target_address;
		/* 21:0 Max IBI Payload Size */
		*(ptr_32 += 1) = DEFAULT_MAX_IBI_PAYLOAD_SIZE;
		/* 31:16 PID_LO, 15:8 DCR/LVR, 7:0 BCR */
		*(ptr_32 += 1) = (pid_lo << 16) + (dcr << 8) + bcr;
		/* 31:0 PID_HI */
		*(ptr_32 += 1) = pid_hi;

		target_address++;
		pid_lo++;
		dcr++;
		bcr++;
	}

	return buffer;
}

static int create_target_device_config_buffer(unsigned char **buffer, uint8_t first_address, int num_entries, uint8_t ibit, uint8_t crr, uint8_t tir, uint32_t payload_size)
{
	uint32_t *ptr_32 = NULL;
	int buffer_size = 0;
	const int config_cmd_type = CHANGE_CONFIG_COMMAND_TYPE;
	uint8_t address = first_address;

	/* a Target Device Config data structure is at least 3 DW long:
	 * Header (1 DW) + Config Change N Entry (2 DW) */
	buffer_size = (1 * DWORD_SIZE) + ((2 * DWORD_SIZE) * num_entries);
	*buffer = (unsigned char *)calloc(1, buffer_size);

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)*buffer;

	/* 31:16 Reserved, 15:8 NumEntries, 7:4 Reserved, 3:0 Config cmd */
	*(ptr_32 += 0) = (num_entries << 8) + config_cmd_type;

	for (int i = 0; i < num_entries; i++) {
		/* 31:11 Reserved, 10 IBI T, 9 CR R, 8 TI R, 7:0 Target Address */
		*(ptr_32 += 1) = (ibit << 10) + (crr << 9) + (tir << 8) + address;
		/* 31:0 Max IBI Payload Size */
		*(ptr_32 += 1) = payload_size;
		address++;
	}

	return buffer_size;
}

static int create_address_change_buffer(unsigned char **buffer, uint8_t current_address, uint8_t new_address)
{
	uint32_t *ptr_32 = NULL;
	int buffer_size = 0;

	const int NUM_ENTRIES = 1;
	const int COMMAND_TYPE = ADDRESS_CHANGE_COMMAND_TYPE;
	const uint16_t PID_LO = INITIAL_PID_LO_POOL;
	const uint32_t PID_HI = 0;

	/* an address change data structure for changing the address
	 * of 1 entry is 3 DW long: Header (1 DW) + Entry body (2 DW) */
	buffer_size = (1 * DWORD_SIZE) + (2 * DWORD_SIZE);
	*buffer = (unsigned char *)calloc(1, buffer_size);

	/* cast the pointer to 32-bit chunks */
	ptr_32 = (uint32_t *)*buffer;

	/* 31:16 Reserved, 15:8 NumEntries, 7:4 Reserved, 3:0 Addr cmd */
	*(ptr_32 += 0) = (NUM_ENTRIES << 8) + COMMAND_TYPE;
	/* 31:16 PID LO, 15:8 New Address, 7:0 Current Address */
	*(ptr_32 += 1) = (PID_LO << 16) + (new_address << 8) + current_address;
	/* 31:0 PID HI */
	*(ptr_32 += 1) = PID_HI;

	return buffer_size;
}

/**************************
 * Mocks
 ***************************/

void mock_clear_feature(int feature_selector, int return_code)
{
	struct control_transfer_mock out;

	out.bmRequestType = 0b00100001;
	out.bRequest = CLEAR_FEATURE;
	out.wValue = feature_selector;
	out.wIndex = feature_selector == HDR_MODE_EXIT_RECOVERY ? 0x7E00 : 0;
	out.wLength = 0;
	out.data = NULL;
	out.timeout = test_timeout;

	mock_libusb_control_transfer(NULL, &out, return_code);
}

void mock_set_feature(int feature_selector, int return_code)
{
	struct control_transfer_mock out;

	out.bmRequestType = 0b00100001;
	out.bRequest = SET_FEATURE;
	out.wValue = feature_selector;
	out.wIndex = 0;
	out.wLength = 0;
	out.data = NULL;
	out.timeout = test_timeout;

	mock_libusb_control_transfer(NULL, &out, return_code);
}

unsigned char *mock_get_i3c_capability(void *fake_handle, int error_code, int device_role, int data_type, int capability, int num_entries, int return_code)
{
	struct control_transfer_mock in;
	unsigned char *cap_buffer = NULL;

	cap_buffer = create_i3c_capability_buffer(error_code, device_role, data_type, USBI3C_DEVICE_STATIC_ADDRESS, capability, num_entries);

	in.bmRequestType = 0b10100001;
	in.timeout = test_timeout;
	in.wValue = 0;
	in.wIndex = 0;
	in.wLength = USB_MAX_CONTROL_BUFFER_SIZE;
	in.bRequest = GET_I3C_CAPABILITY;
	in.data = cap_buffer;

	mock_libusb_control_transfer(fake_handle, &in, return_code);

	return cap_buffer;
}

void mock_initialize_i3c_bus(void *fake_handle, int mode, int return_code)
{
	struct control_transfer_mock out;

	out.bmRequestType = 0b00100001;
	out.bRequest = INITIALIZE_I3C_BUS;
	out.wValue = mode;
	out.wIndex = 0;
	out.wLength = 0;
	out.data = NULL; // shall not send Device Table if the Controller has the knowledge of Target devices on the I3C Bus
	out.timeout = test_timeout;

	mock_libusb_control_transfer(fake_handle, &out, return_code);
}

unsigned char *mock_get_target_device_table(void *fake_handle, int num_entries, int target_config, int target_capability, int return_code)
{
	struct control_transfer_mock in;
	unsigned char *target_dev_table_buffer = NULL;

	target_dev_table_buffer = create_target_device_table_buffer(num_entries, target_config, target_capability);

	in.bmRequestType = 0b10100001;
	in.timeout = test_timeout;
	in.wValue = 0;
	in.wIndex = 0;
	in.wLength = USB_MAX_CONTROL_BUFFER_SIZE;
	in.bRequest = GET_TARGET_DEVICE_TABLE;
	in.data = target_dev_table_buffer;

	mock_libusb_control_transfer(fake_handle, &in, return_code);

	return target_dev_table_buffer;
}

static unsigned char *set_target_device_config(void *fake_handle, uint8_t address, int num_entries, uint8_t ibit, uint8_t crr, uint8_t tir, uint32_t payload_size, int return_code)
{
	struct control_transfer_mock out;
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	buffer_size = create_target_device_config_buffer(&buffer, address, num_entries, ibit, crr, tir, payload_size);

	out.bmRequestType = 0b00100001;
	out.bRequest = SET_TARGET_DEVICE_CONFIG;
	out.wValue = 0;
	out.wIndex = 0;
	out.wLength = buffer_size;
	out.data = buffer;
	out.timeout = test_timeout;

	mock_libusb_control_transfer(fake_handle, &out, return_code);

	return buffer;
}

unsigned char *mock_set_target_device_config(uint8_t address, uint8_t ibit, uint8_t crr, uint8_t tir, int return_code)
{
	return set_target_device_config(NULL, address, SINGLE_ADDRESS, ibit, crr, tir, DEFAULT_MAX_IBI_PAYLOAD_SIZE, return_code);
}

unsigned char *mock_set_target_max_ibi_payload(uint8_t address, uint32_t payload_size, int return_code)
{
	const int DEFAULT_CONF = 0;

	return set_target_device_config(NULL, address, SINGLE_ADDRESS, DEFAULT_CONF, DEFAULT_CONF, DEFAULT_CONF, payload_size, return_code);
}

unsigned char *mock_set_target_device_configs_from_device_capability(void *fake_handle, uint8_t address_start, int num_entries, int dev_capability, uint32_t payload_size, int return_code)
{
	uint8_t crr = !(dev_capability & 0b001);
	uint8_t tir = !(dev_capability & 0b100);

	return set_target_device_config(fake_handle, address_start, num_entries, 0, crr, tir, payload_size, return_code);
}

unsigned char *mock_change_dynamic_address(uint8_t old_address, uint8_t new_address, int return_code)
{
	struct control_transfer_mock out;
	unsigned char *buffer = NULL;
	int buffer_size = 0;

	buffer_size = create_address_change_buffer(&buffer, old_address, new_address);

	out.bmRequestType = 0b00100001;
	out.bRequest = CHANGE_DYNAMIC_ADDRESS;
	out.wValue = 0;
	out.wIndex = 0;
	out.wLength = buffer_size;
	out.data = buffer;
	out.timeout = test_timeout;

	mock_libusb_control_transfer(NULL, &out, return_code);

	return buffer;
}

unsigned char *mock_get_address_change_result(uint8_t old_address, uint8_t new_address, int address_change_status, int return_code)
{
	struct control_transfer_mock in;
	unsigned char *buffer = NULL;
	int buffer_size = 0;
	const int NUM_ENTRIES = 1;

	buffer_size = helper_create_address_change_result_buffer(&buffer, old_address, new_address, NUM_ENTRIES, address_change_status);

	in.bmRequestType = 0b10100001;
	in.bRequest = GET_ADDRESS_CHANGE_RESULT;
	in.wValue = 0;
	in.wIndex = 0;
	in.wLength = buffer_size;
	in.data = buffer;
	in.timeout = test_timeout;

	mock_libusb_control_transfer(NULL, &in, return_code);

	return buffer;
}

void mock_get_buffer_available(void *fake_handle, int *buffer_available, int return_code)
{
	struct control_transfer_mock in;

	in.bmRequestType = 0b10100001;
	in.bRequest = GET_BUFFER_AVAILABLE;
	in.wValue = 0;
	in.wIndex = 0;
	in.wLength = 4;
	in.data = (unsigned char *)buffer_available;
	in.timeout = test_timeout;

	mock_libusb_control_transfer(fake_handle, &in, return_code);
}

void mock_cancel_or_resume_bulk_request(int return_code)
{
	/* this request is going to be submitted in an async transfer, so we not
	 * only need to mock the outgoing control transfer, but we also have to mock
	 * the event that is going to signal that the async transfer has been completed */
	mock_usb_output_control_transfer_async(return_code);
	mock_usb_wait_for_next_event(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, NULL, 0, return_code);
}
