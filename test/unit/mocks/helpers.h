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

#ifndef __unit_test_helpers_h__
#define __unit_test_helpers_h__

#include <libusb.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

#include "usbi3c_i.h"

#define RETURN_SUCCESS 0
#define RETURN_FAILURE -1

#define USB_ENDPOINT_MASK 0x0F

/* holds the current request ID */
extern uint16_t bulk_request_id;

/* flags used to enable mocks disabled by default */
extern int enable_mock_libusb_alloc_transfer;
extern int enable_mock_libusb_free_transfer;

/* test_helpers.c */
struct usbi3c_device *helper_usbi3c_init(void *fake_handle);
void helper_usbi3c_deinit(struct usbi3c_device **usbi3c, void *fake_handle);
struct list *helper_create_test_list(int a, int b);
void helper_create_dummy_devices_in_target_device_table(struct usbi3c_device *usbi3c_dev, int number_of_devices);
int helper_initialize_controller(struct usbi3c_device *usbi3c, void *fake_handle, uint8_t **address_list);
void helper_initialize_target_device(struct usbi3c_device *usbi3c, void *handle);
void helper_trigger_incoming_transfer(int endpoint, unsigned char *buffer, int buffer_size);
void helper_trigger_notification(unsigned char *notification_buffer, int buffer_size);
void helper_trigger_response(unsigned char *response_buffer, int buffer_size);
void helper_trigger_control_transfer(unsigned char *control_transfer_buffer, int buffer_size);
size_t helper_create_notification_buffer(uint8_t **buffer, int type, int code);
int helper_create_address_change_result_buffer(unsigned char **buffer, uint8_t current_address, uint8_t new_address, int num_entries, int status);

/* fake_transfer_helpers.c */
void fake_transfer_init(void);
void fake_transfer_deinit(void);
void fake_transfer_add_data(int endpoint, unsigned char *data, int size);
void fake_transfer_set_transaction_status(int endpoint, int status);
void fake_transfer_add_transfer(int endpoint, struct libusb_transfer *transfer);
struct libusb_transfer *fake_transfer_get_transfer(int endpoint);
void fake_transfer_emit(void);
void fake_transfer_trigger(int endpoint);

/* command_helpers.c */
int helper_create_ccc_with_defining_byte_buffer(int request_id, int ccc, int defining_byte, unsigned char **buffer, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous);
int helper_create_ccc_buffer(int request_id, int ccc, unsigned char **buffer, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous);
int helper_create_command_buffer(int request_id, unsigned char **buffer, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data, int transfer_mode, int transfer_rate, uint8_t dependent_on_previous);
int helper_add_ccc_with_defining_byte_to_command_buffer(int request_id, int ccc, int defining_byte, unsigned char **buffer, int current_buffer_size, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data);
int helper_add_ccc_to_command_buffer(int request_id, int ccc, unsigned char **buffer, int current_buffer_size, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data);
int helper_add_to_command_buffer(int request_id, unsigned char **buffer, int current_buffer_size, int target_address, int command_direction, int error_handling, int data_size, unsigned char *data);
int helper_add_target_reset_pattern_to_command_buffer(int request_id, unsigned char **buffer, int current_buffer_size);
struct usbi3c_command *helper_create_command(on_response_fn on_response_cb, void *user_data, unsigned char **buffer, int *buffer_size, int *request_id);
struct list *helper_create_commands(on_response_fn on_response_cb, void *user_data, unsigned char **buffer, int *buffer_size, int *request_id, uint8_t dependent_on_previous);
int helper_create_response_buffer(unsigned char **buffer, struct usbi3c_response *response, int request_id);
int helper_create_multiple_response_buffer(unsigned char **buffer, struct list *responses, int request_id);
void helper_add_request_to_tracker(struct request_tracker *request_tracker, int request_id, int total_commands, struct usbi3c_response *response);

#endif // __unit_test_helpers_h__
