/***************************************************************************
  usbi3c.c  -  description
  -------------------
  copyright            : (C) 2022 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ibi_i.h"
#include "ibi_response_i.h"
#include "target_device_table_i.h"
#include "usb_i.h"
#include "usbi3c_i.h"
#include "usbi3c_version_i.h"

// function to handle bus initialization status notification
static void bus_init_status_notification_handle(struct notification *notification, void *user_data)
{
	struct usbi3c_device *usbi3c_dev = (struct usbi3c_device *)user_data;
	pthread_mutex_lock(&usbi3c_dev->lock);
	usbi3c_dev->bus_init_status = notification->code;
	pthread_mutex_unlock(&usbi3c_dev->lock);
}

// Function to handle bus error notification
static void bus_error_notification_handle(struct notification *notification, void *user_data)
{
	struct usbi3c_device *usbi3c_dev = (struct usbi3c_device *)user_data;
	struct bus_error_handler *bus_error_handler = &usbi3c_dev->bus_error_handler;
	pthread_mutex_lock(&usbi3c_dev->lock);
	if (bus_error_handler->on_bus_error_cb != NULL) {
		bus_error_handler->on_bus_error_cb(notification->code, bus_error_handler->data);
	}
	pthread_mutex_unlock(&usbi3c_dev->lock);
}

// This function increments the reference counter of an usbi3c context
static struct usbi3c_context *usbi3c_ref_context(struct usbi3c_context *usbi3c_ctx)
{
	if (usbi3c_ctx) {
		usbi3c_ctx->ref_count++;
	}

	return usbi3c_ctx;
}

// This function decrements the reference counter of an usbi3c context
static void usbi3c_unref_context(struct usbi3c_context *usbi3c_ctx)
{
	if (usbi3c_ctx) {
		usbi3c_ctx->ref_count--;
	}
}

// Function to associate notification handler info with notification type easier
static void usbi3c_add_notification_handler(struct usbi3c_device *usbi3c_dev,
					    uint8_t notification_type,
					    void (*handle)(struct notification *notification,
							   void *user_data),
					    void *user_data)
{
	usbi3c_dev->handlers[notification_type].user_data = user_data;
	usbi3c_dev->handlers[notification_type].handle = handle;
}

// This function creates a new usbi3c device from a given USB device and usbi3c context
static struct usbi3c_device *usbi3c_device_create(struct usbi3c_context *usb_ctx, struct usb_device *usb_dev)
{
	struct usbi3c_device *usbi3c_dev = malloc_or_die(sizeof(struct usbi3c_device));

	pthread_mutex_init(&usbi3c_dev->lock, NULL);
	usb_set_interrupt_context(usb_dev, usbi3c_dev);
	usbi3c_dev->device_info = NULL;
	usbi3c_dev->target_device_table = table_init(usb_dev);
	if (usbi3c_dev->target_device_table == NULL) {
		goto FREE_AND_EXIT;
	}
	usbi3c_add_notification_handler(usbi3c_dev,
					NOTIFICATION_ADDRESS_CHANGE_STATUS,
					target_device_table_notification_handle,
					usbi3c_dev->target_device_table);

	usbi3c_add_notification_handler(usbi3c_dev,
					NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS,
					bus_init_status_notification_handle,
					usbi3c_dev);

	usbi3c_add_notification_handler(usbi3c_dev,
					NOTIFICATION_I3C_BUS_ERROR,
					bus_error_notification_handle,
					usbi3c_dev);

	struct ibi_response_queue *response_queue = ibi_response_queue_get_queue();
	usbi3c_dev->ibi = ibi_init(response_queue);
	if (usbi3c_dev->ibi == NULL) {
		goto FREE_AND_EXIT;
	}
	usbi3c_add_notification_handler(usbi3c_dev,
					NOTIFICATION_I3C_IBI,
					ibi_handle_notification,
					usbi3c_dev->ibi);

	/* initialize the structs required for bulk transfers */
	usbi3c_dev->i3c_mode = i3c_mode_init();
	usbi3c_dev->command_queue = NULL;
	usbi3c_dev->request_tracker = bulk_transfer_request_tracker_init(usb_dev, response_queue, usbi3c_dev->ibi);
	usbi3c_add_notification_handler(usbi3c_dev, NOTIFICATION_STALL_ON_NACK, stall_on_nack_handle, usbi3c_dev->request_tracker);
	usb_set_bulk_transfer_context(usb_dev, usbi3c_dev->request_tracker);
	usbi3c_dev->usb_dev = usb_device_ref(usb_dev);
	usbi3c_dev->usbi3c_ctx = usbi3c_ref_context(usb_ctx);
	usbi3c_ref_device(usbi3c_dev);
	return usbi3c_dev;

FREE_AND_EXIT:
	usbi3c_device_deinit(&usbi3c_dev);
	return NULL;
}

/**
 * @ingroup library_setup
 * @brief This function returns a list of all usbi3c devices matching the given vendor and product IDs.
 *
 * @param[in] usbi3c_ctx The usbi3c context
 * @param[in] vendor_id The vendor ID to match (use 0 for any)
 * @param[in] product_id The product ID to match (use 0 for any)
 * @param[out] usbi3c_devs The list of usbi3c devices
 * @return number of usbi3c devices found or -1 on error
 */
int usbi3c_get_devices(struct usbi3c_context *usbi3c_ctx, const uint16_t vendor_id, const uint16_t product_id, struct usbi3c_device ***usbi3c_devs)
{
	struct usb_search_criteria i3c_search_criteria = {
		.dev_class = USBI3C_DeviceClass,
		.vendor_id = vendor_id,
		.product_id = product_id,
	};
	struct usb_device **usb_devices = NULL;
	struct usbi3c_device **usbi3c_devices = NULL;
	int ret = -1;
	int device_count = 0;

	if (usbi3c_ctx == NULL) {
		DEBUG_PRINT("The usbi3c context is missing, aborting...\n");
		return -1;
	}

	/* search for a USB that matches the I3C device criteria */
	ret = usb_find_devices(usbi3c_ctx->usb_ctx, &i3c_search_criteria, &usb_devices);
	if (ret < 0) {
		DEBUG_PRINT("Error while searching for USBI3C devices\n");
		return -1;
	}

	usbi3c_devices = malloc_or_die((ret + 1) * sizeof(struct usbi3c_device *));
	for (int i = 0; i < ret; i++) {
		usb_device_init(usb_devices[i]);
		struct usbi3c_device *usbi3c_dev = usbi3c_device_create(usbi3c_ctx, usb_devices[i]);
		if (usbi3c_dev != NULL) {
			usbi3c_devices[device_count++] = usbi3c_dev;
		}
	}

	if (device_count == 0) {
		DEBUG_PRINT("No USBI3C devices found\n");
		goto FREE_AND_EXIT;
	}

	usbi3c_devices[device_count] = NULL;
	usbi3c_devices = realloc_or_die(usbi3c_devices, (device_count + 1) * sizeof(struct usbi3c_device *));
	*usbi3c_devs = usbi3c_devices;

	usb_free_devices(usb_devices, ret);

	return device_count;
FREE_AND_EXIT:
	usb_free_devices(usb_devices, ret);
	FREE(usbi3c_devices);
	return 0;
}

/**
 * @ingroup library_setup
 * @brief Initialize @lib_name.
 *
 * @return usbi3c_context on success, or NULL on failure
 * @remark This is the entry point for this library
 * @note This function does not initialize the I3C bus
 */
struct usbi3c_context *usbi3c_init(void)
{
	struct usbi3c_context *usbi3c = NULL;

	usbi3c = (struct usbi3c_context *)malloc_or_die(sizeof(struct usbi3c_context));

	/* initialize a usb context */
	int ret = usb_context_init(&usbi3c->usb_ctx);
	if (ret < 0) {
		usbi3c_deinit(&usbi3c);
		return NULL;
	}
	usbi3c_ref_context(usbi3c);

	/* Return the created context */
	return usbi3c;
}

/**
 * @ingroup library_setup
 * @brief Deinitialize @lib_name.
 *
 * @param[in] usbi3c the context to deinitialize
 * @remark This function should be called after closing all open devices and
 * before your application terminates.
 */
void usbi3c_deinit(struct usbi3c_context **usbi3c)
{
	if (usbi3c == NULL || *usbi3c == NULL) {
		return;
	}

	usbi3c_unref_context(*usbi3c);
	if ((*usbi3c)->ref_count > 0) {
		return;
	}

	if ((*usbi3c)->usb_ctx) {
		usb_context_deinit((*usbi3c)->usb_ctx);
	}

	FREE(*usbi3c);

	return;
}

/**
 * @ingroup library_setup
 * @brief Increment the reference count of the given usbi3c device
 *
 * @param[in] usbi3c_dev The usbi3c device
 * @return The usbi3c device
 */
struct usbi3c_device *usbi3c_ref_device(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		return NULL;
	}

	usbi3c_dev->ref_count++;
	return usbi3c_dev;
}

/**
 * @ingroup library_setup
 * @brief Decrement the reference count of the given usbi3c device
 *
 * @param[in] usbi3c_dev The usbi3c device
 */
void usbi3c_unref_device(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		return;
	}
	usbi3c_dev->ref_count--;
}

/**
 * @ingroup library_setup
 * @brief Free the given usbi3c device list, deinit all devices and assign NULL to the list
 *
 * @param[in] devices The usbi3c device list
 */
void usbi3c_free_devices(struct usbi3c_device ***devices)
{
	if (devices == NULL || *devices == NULL) {
		return;
	}

	struct usbi3c_device **usbi3c_devices = *devices;
	for (int i = 0; usbi3c_devices[i] != NULL; i++) {
		usbi3c_device_deinit(&usbi3c_devices[i]);
	}

	FREE(usbi3c_devices);
	*devices = NULL;
}

/**
 * @ingroup library_setup
 * @brief This function deinitalize a usbi3c device
 *
 * @param[in] usbi3c_dev The usbi3c device to deinitialize
 */
void usbi3c_device_deinit(struct usbi3c_device **usbi3c_dev)
{
	if (usbi3c_dev == NULL || *usbi3c_dev == NULL) {
		return;
	}

	usbi3c_unref_device(*usbi3c_dev);
	if ((*usbi3c_dev)->ref_count > 0) {
		return;
	}

	if ((*usbi3c_dev)->usb_dev) {
		usb_device_deinit((*usbi3c_dev)->usb_dev);
	}

	if ((*usbi3c_dev)->device_info) {
		FREE((*usbi3c_dev)->device_info);
	}

	if ((*usbi3c_dev)->target_device_table) {
		table_destroy(&(*usbi3c_dev)->target_device_table);
	}

	if ((*usbi3c_dev)->ibi) {
		ibi_destroy(&(*usbi3c_dev)->ibi);
	}

	if ((*usbi3c_dev)->request_tracker) {
		request_tracker_destroy(&(*usbi3c_dev)->request_tracker);
	}

	if ((*usbi3c_dev)->device_event_handler) {
		device_destroy_event_handler(&(*usbi3c_dev)->device_event_handler);
	}

	if ((*usbi3c_dev)->command_queue) {
		bulk_transfer_free_commands(&(*usbi3c_dev)->command_queue);
	}

	if ((*usbi3c_dev)->i3c_mode) {
		FREE((*usbi3c_dev)->i3c_mode);
	}

	pthread_mutex_destroy(&(*usbi3c_dev)->lock);
	usbi3c_deinit(&(*usbi3c_dev)->usbi3c_ctx);
	FREE(*usbi3c_dev);

	return;
}

/**
 * @brief Sets a default configuration in all target devices in the table.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the devices are configured correctly, or -1 otherwise
 */
static int usbi3c_set_default_target_device_config(struct usbi3c_device *usbi3c_dev)
{
	uint8_t *buffer = NULL;
	uint16_t buffer_size = 0;
	uint8_t config = 0xFF;

	if (usbi3c_dev->device_info->capabilities.handoff_controller_role) {
		config &= ~CONTROLLER_ROLE_REQUEST_MASK;
	}

	if (usbi3c_dev->device_info->capabilities.in_band_interrupt_capability) {
		config &= ~TARGET_INTERRUPT_REQUEST_MASK;
	}

	buffer_size = table_create_set_target_config_buffer(usbi3c_dev->target_device_table, config, usbi3c_dev->device_info->capabilities.max_ibi_payload_size, &buffer);

	if (usb_output_control_transfer(usbi3c_dev->usb_dev, SET_TARGET_DEVICE_CONFIG, 0, USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, buffer, buffer_size) < 0) {
		DEBUG_PRINT("Error while setting the default target device configuration\n");
		FREE(buffer);
		return -1;
	}
	/* get the updated target device table from the I3C controller */
	if (table_update_target_device_info(usbi3c_dev->target_device_table)) {
		DEBUG_PRINT("The target device table could not be retrieved, aborting...\n");
		FREE(buffer);
		return -1;
	}

	FREE(buffer);

	return 0;
}

// function to send initialization request to USBI3C device
static int initialize_i3c_bus(struct usbi3c_device *usbi3c_dev)
{
	uint16_t buffer_size = 0;
	uint8_t *buffer = NULL;
	int devices_supporting_static = 0;
	int devices_supporting_dynamic = 0;
	int mode;

	if (usbi3c_get_device_role(usbi3c_dev) != USBI3C_PRIMARY_CONTROLLER_ROLE) {
		DEBUG_PRINT("This request is only supported by the primary I3C controller\n");
		return -1;
	}

	/* we need to decide the dynamic address assignment mode to use based on:
	 * - wether or not the I3C controller is aware of target devices on the bus
	 * - the type of target devices on the bus (I2C/I3C/I3C with static address) */
	if (usbi3c_dev->device_info->data_type == STATIC_DATA || usbi3c_dev->device_info->data_type == DYNAMIC_DATA) {
		/* the controller is aware of target devices, we can let it decide */
		mode = I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT;
	} else {
		/* the controller is not aware of the target devices */
		if (usbi3c_dev->target_device_table->target_devices != NULL) {
			if (table_identify_devices(usbi3c_dev->target_device_table, &devices_supporting_static, &devices_supporting_dynamic) < 0) {
				DEBUG_PRINT("There was an error identifying the type of devices in the table\n");
				return -1;
			}

			if (devices_supporting_static > 0 && devices_supporting_dynamic == 0) {
				/* only I2C-capable devices in the table */
				mode = SET_STATIC_ADDRESS_AS_DYNAMIC_ADDRESS;
			} else if (devices_supporting_dynamic > 0 && devices_supporting_static == 0) {
				/* only I3C devices in the table */
				mode = ENTER_DYNAMIC_ADDRESS_ASSIGNMENT;
			} else {
				/* there is a mix of devices in the table, let's allow the controller to decide */
				mode = I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT;
			}
		} else {
			/* if the controller is unaware of target devices, and the user didn't provide
			 * a target device table, we have no other option but broadcast the ENTDAA to
			 * the I3C devices to enter dynamic address assignment, and go fishing */
			mode = ENTER_DYNAMIC_ADDRESS_ASSIGNMENT;
		}
	}

	return usb_output_control_transfer(usbi3c_dev->usb_dev,
					   INITIALIZE_I3C_BUS,
					   mode,
					   USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
					   buffer,
					   buffer_size);
}

/**
 * @brief Initializes the usbi3c device as I3C controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the I3C controller was initialized correctly, or -1 otherwise
 */
static int usbi3c_initialize_controller(struct usbi3c_device *usbi3c_dev)
{
	int init_status = I3C_BUS_UNINITIALIZED;

	/* initialize the I3C bus */
	if (initialize_i3c_bus(usbi3c_dev)) {
		DEBUG_PRINT("The I3C bus could not be initialized, aborting...\n");
		return -1;
	}

	while (init_status == I3C_BUS_UNINITIALIZED) {
		usb_wait_for_next_event(usbi3c_dev->usb_dev);
		pthread_mutex_lock(&usbi3c_dev->lock);
		init_status = usbi3c_dev->bus_init_status;
		pthread_mutex_unlock(&usbi3c_dev->lock);
	}

	if (init_status != SUCCESSFUL_I3C_BUS_INITIALIZATION) {
		DEBUG_PRINT("The I3C controller encountered a failure initializing the bus, aborting...\n");
		return -1;
	}

	/* get the updated target device table from the I3C controller */
	if (table_update_target_device_info(usbi3c_dev->target_device_table)) {
		DEBUG_PRINT("The target device table could not be retrieved, aborting...\n");
		return -1;
	}

	/* set an initial configuration in all I3C target devices using the I3C controller
	 * capabilities */
	if (usbi3c_set_default_target_device_config(usbi3c_dev)) {
		DEBUG_PRINT("The table devices configuration has failed, aborting...\n");
		return -1;
	}

	table_enable_events(usbi3c_dev->target_device_table);

	usbi3c_dev->device_info->device_state.active_i3c_controller = TRUE;

	return 0;
}

/**
 * @brief Initializes the usbi3c device as I3C target device or target device capable of secondary controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the I3C device was initialized correctly, or -1 otherwise
 */
static int usbi3c_initialize_target_device(struct usbi3c_device *usbi3c_dev)
{
	/* enable the target device event handler for I3C controller events */
	usbi3c_dev->device_event_handler = device_event_handler_init();
	if (usbi3c_dev->device_event_handler == NULL) {
		DEBUG_PRINT("The target device event handler failed to be initialized, aborting...\n");
		return -1;
	}

	if (usbi3c_get_device_role(usbi3c_dev) == USBI3C_PRIMARY_CONTROLLER_ROLE) {
		DEBUG_PRINT("The I3C device is an I3C controller not a Target Device, aborting...\n");
		return -1;
	}
	usbi3c_add_notification_handler(usbi3c_dev,
					NOTIFICATION_ACTIVE_I3C_CONTROLLER_EVENT,
					device_handle_event,
					usbi3c_dev->device_event_handler);

	/* send a command to hot-join an already initialized I3C bus to the
	 * target device */
	return device_send_request_to_i3c_controller(usbi3c_dev, HOT_JOIN_ADDRESS, USBI3C_WRITE);
}

// function to handle USB interrupts and call the notification associated with it
static void notification_dispatcher(void *arg, uint8_t *buffer, uint16_t length)
{
	struct usbi3c_device *usbi3c_dev = (struct usbi3c_device *)arg;
	struct notification notification = { 0 };

	if (length > NOTIFICATION_SIZE) {
		DEBUG_PRINT("Invalid notification format\n");
		return;
	}

	notification.type = GET_NOTIFICATION_FORMAT(buffer)->type;
	notification.code = GET_NOTIFICATION_FORMAT(buffer)->code;

	if (notification.type >= NOTIFICATION_HANDLERS_SIZE || notification.type == 0) {
		DEBUG_PRINT("Notification type not supported %d will be ignored\n", notification.type);
		return;
	}

	if (usbi3c_dev->handlers[notification.type].handle) {
		usbi3c_dev->handlers[notification.type].handle(&notification,
							       usbi3c_dev->handlers[notification.type].user_data);
	}
}

// function to get device info from get_capability response
static struct device_info *device_info_create_from_capability_buffer(uint8_t *buffer)
{
	struct device_info *device = NULL;
	uint8_t error_code = 0;

	/* Get the data from the capability header buffer first, the I3C may not
	 * contain capability data and in that case only the header will be returned */
	error_code = GET_CAPABILITY_HEADER(buffer)->error_code;

	if (error_code != DEVICE_CONTAINS_CAPABILITY_DATA && error_code != DEVICE_DOES_NOT_CONTAIN_CAPABILITY_DATA) {
		DEBUG_PRINT("Unknown get capability error: %d\n", error_code);
		return NULL;
	}

	device = (struct device_info *)malloc_or_die(sizeof(struct device_info));

	if (error_code == DEVICE_DOES_NOT_CONTAIN_CAPABILITY_DATA) {
		/* the host shall assume the I3C Controller role for the I3C device
		 * with no knowledge of target devices on I3C Bus */
		device->device_role = USBI3C_PRIMARY_CONTROLLER_ROLE;
		device->data_type = NO_STATIC_DATA;
		return device;
	}

	device->device_role = GET_CAPABILITY_HEADER(buffer)->device_role;
	device->data_type = GET_CAPABILITY_HEADER(buffer)->data_type;
	device->address = GET_CAPABILITY_BUS(buffer)->i3c_device_address;
	device->capabilities.devices_present = GET_CAPABILITY_BUS(buffer)->devices_present;
	device->capabilities.handoff_controller_role = GET_CAPABILITY_BUS(buffer)->handoff_controller_role;
	device->capabilities.hot_join_capability = GET_CAPABILITY_BUS(buffer)->hot_join_capability;
	device->capabilities.in_band_interrupt_capability = GET_CAPABILITY_BUS(buffer)->in_band_interrupt_capability;
	device->capabilities.pending_read_capability = GET_CAPABILITY_BUS(buffer)->pending_read_capability;
	device->capabilities.self_initiated = GET_CAPABILITY_BUS(buffer)->self_initiated;
	device->capabilities.delayed_pending_read = GET_CAPABILITY_BUS(buffer)->delayed_pending_read;
	device->capabilities.pending_read_sdr = GET_CAPABILITY_BUS(buffer)->pending_read_sdr;
	device->capabilities.pending_read_hdr = GET_CAPABILITY_BUS(buffer)->pending_read_hdr;
	device->capabilities.single_command_pending_read = GET_CAPABILITY_BUS(buffer)->single_command_pending_read;
	device->capabilities.i3c_minor_ver = GET_CAPABILITY_BUS(buffer)->mipi_minor_version;
	device->capabilities.i3c_major_ver = GET_CAPABILITY_BUS(buffer)->mipi_major_version;
	device->capabilities.disco_minor_ver = GET_CAPABILITY_BUS(buffer)->mipi_disco_minor_version;
	device->capabilities.disco_major_ver = GET_CAPABILITY_BUS(buffer)->mipi_disco_major_version;
	device->capabilities.i2c_data_transfer_rates = GET_CAPABILITY_BUS(buffer)->i2c_data_transfer_rates;
	device->capabilities.clock_frequency_i2c_udr1 = GET_CAPABILITY_BUS(buffer)->clock_frequency_i2c_udr1;
	device->capabilities.clock_frequency_i2c_udr2 = GET_CAPABILITY_BUS(buffer)->clock_frequency_i2c_udr2;
	device->capabilities.clock_frequency_i2c_udr3 = GET_CAPABILITY_BUS(buffer)->clock_frequency_i2c_udr3;
	device->capabilities.i3c_data_transfer_modes = GET_CAPABILITY_BUS(buffer)->i3c_data_transfer_modes;
	device->capabilities.i3c_data_transfer_rates = GET_CAPABILITY_BUS(buffer)->i3c_data_transfer_rates;
	device->capabilities.transfer_mode_extended_capability_length = GET_CAPABILITY_BUS(buffer)->transfer_mode_extended_cap_len;
	device->capabilities.max_ibi_payload_size = GET_CAPABILITY_BUS(buffer)->max_ibi_payload_size;

	/* initialize the device state */
	device->device_state.active_i3c_controller = FALSE;
	device->device_state.handoff_controller_role_enabled = FALSE;
	device->device_state.hot_join_enabled = FALSE;
	device->device_state.in_band_interrupt_enabled = FALSE;

	return device;
}

/**
 * @ingroup library_setup
 * @brief Initialize the usbi3c device depending on its capabilities.
 *
 * The I3C Device inside a USB Device shall support one of the following three roles:
 * - I3C Controller role
 * - I3C Target device role
 * - I3C Target device capable of Secondary Controller role
 *
 * If the device's role is I3C Controller, it performs the initial configuration of the
 * I3C Bus and all the Target devices on the I3C Bus, including the dynamic address
 * assignment of the I3C Target devices.
 *
 * If the device's role is I3C Target device, on Host’s request I3C Target device Hot-Joins
 * an already initialized I3C Bus.
 *
 * These are the steps performed as part of the initialization:
 * - Gets the capabilities of the I3C device and target devices
 * - Starts listening for USB interrupts
 * - Starts listening for bulk response transfers
 * - Initializes the I3C bus (I3C Controller role only)
 *   - The I3C controller performs I3C target device discovery
 *   - I3C function generates and stores the target device table
 * - Gets the updated target device table (I3C Controller role only)
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the device was initalized correctly, or -1 otherwise
 */
int usbi3c_initialize_device(struct usbi3c_device *usbi3c_dev)
{
	/* We cannot know in advanced how big the control transfer input buffer needs to be,
	 * we won't know the size of the I3C Capability data structure until after
	 * we have received the control transfer response. So to get a suitably-sized buffer for the
	 * transfer, we'll just use the maximum buffer size possible for that type of transaction. */
	uint8_t cap_buffer[USB_MAX_CONTROL_BUFFER_SIZE] = { 0 };
	unsigned char *buffer = NULL;
	uint32_t buffer_size = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	if (usbi3c_dev->usb_dev == NULL || !usb_device_is_initialized(usbi3c_dev->usb_dev)) {
		DEBUG_PRINT("The USB device is not initialized, aborting...\n");
		return -1;
	}

	/* get the capability of the I3C device to determine its role, and in the case of the
	 * controller role, fill up the target device table (if the controller is aware of the
	 * target devices on the bus, if it is not, we just keep an empty table). */
	if (usb_input_control_transfer(usbi3c_dev->usb_dev,
				       GET_I3C_CAPABILITY,
				       0,
				       USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
				       cap_buffer,
				       USB_MAX_CONTROL_BUFFER_SIZE) < 0) {
		DEBUG_PRINT("The I3C capabilities of the device could not be read, aborting...\n");
		return -1;
	}

	/* parse the capability of the I3C controller device from the buffer */
	usbi3c_dev->device_info = device_info_create_from_capability_buffer(cap_buffer);
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The I3C device info could not be read, aborting...\n");
		return -1;
	}

	/* parse the info from each target device (if it exists) from the buffer
	 * and fill up the target device table with it */
	if (table_fill_from_capability_buffer(usbi3c_dev->target_device_table, cap_buffer,
					      GET_CAPABILITY_HEADER(cap_buffer)->total_length)) {
		DEBUG_PRINT("The target device table could not be filled, aborting...\n");
		return -1;
	}

	/* the I3C function will communicate with the host certain information like responses to commands
	 * and vendor specific requests, using bulk responses. We cannot know when those responses will
	 * be sent, so we need to poll the bulk response request endpoint to get them */
	buffer_size = usb_bulk_transfer_response_buffer_init(usbi3c_dev->usb_dev, &buffer);
	if (buffer == NULL) {
		DEBUG_PRINT("The bulk response transfer buffer could not be initialized, aborting...\n");
		return -1;
	}

	if (usb_input_bulk_transfer_polling(usbi3c_dev->usb_dev, buffer, buffer_size, bulk_transfer_get_response)) {
		DEBUG_PRINT("There was an error starting the polling mechanism for bulk response transfers, aborting...\n");
		return -1;
	}

	/* initialize the USB interrupt handler to start listening for interrupts */
	usb_set_interrupt_buffer_length(usbi3c_dev->usb_dev, NOTIFICATION_SIZE);
	if (usb_interrupt_init(usbi3c_dev->usb_dev, notification_dispatcher)) {
		DEBUG_PRINT("The USB interrupt handler failed to be initialized, aborting...\n");
		return -1;
	}

	if (usbi3c_get_device_role(usbi3c_dev) == USBI3C_PRIMARY_CONTROLLER_ROLE) {

		return usbi3c_initialize_controller(usbi3c_dev);
	}
	if (usbi3c_get_device_role(usbi3c_dev) == USBI3C_TARGET_DEVICE_ROLE || usbi3c_get_device_role(usbi3c_dev) == USBI3C_TARGET_DEVICE_SECONDARY_CONTROLLER_ROLE) {

		return usbi3c_initialize_target_device(usbi3c_dev);
	}

	DEBUG_PRINT("Unsupported I3C device role: %d\n", usbi3c_get_device_role(usbi3c_dev));

	return -1;
}

/**
 * @brief I3C Device class-specific request used to enable/disable features defined by the value of selector.
 *
 * Refer to the i3c_feature_selector enum for the supported values.
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] request_type defines the type of request, either SET_FEATURE or CLEAR_FEATURE
 * @param[in] selector selectable I3C feature to be enabled or disabled
 * @param[in] address this is the address for features that are enabled/disabled on a specific target device,
 * the target address can also be the broadcast address, for features that are enabled/disabled on the I3C bus
 * this value should be set to 0
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
static int usbi3c_set_feature(struct usbi3c_device *usbi3c_dev, enum i3c_class_request request_type, enum i3c_feature_selector selector, uint8_t address)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == FALSE) {
		DEBUG_PRINT("The I3C device is not the active I3C controller\n");
		return -1;
	}

	return usb_output_control_transfer(usbi3c_dev->usb_dev,
					   request_type,
					   selector,
					   (address << 8) | USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
					   NULL,
					   0);
}

/**
 * @ingroup bus_configuration
 * @brief Enables the I3C Controller role handoff.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_i3c_controller_role_handoff(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.handoff_controller_role == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handing off the I3C Controller role\n");
		return -1;
	}
	if (usbi3c_dev->device_info->device_state.handoff_controller_role_enabled == TRUE) {
		DEBUG_PRINT("The I3C Controller Role Handoff feature is already enabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, SET_FEATURE, I3C_CONTROLLER_ROLE_HANDOFF, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.handoff_controller_role_enabled = TRUE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Enables all regular In-Band Interrupts on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_regular_ibi(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.in_band_interrupt_capability == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handling IBIs\n");
		return -1;
	}
	if (usbi3c_dev->device_info->device_state.in_band_interrupt_enabled == TRUE) {
		DEBUG_PRINT("The In-Band Interrupts feature is already enabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, SET_FEATURE, REGULAR_IBI, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.in_band_interrupt_enabled = TRUE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Enables the Hot-Join feature on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_hot_join(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.hot_join_capability == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handling Hot-Join\n");
		return -1;
	}

	if (usbi3c_dev->device_info->device_state.hot_join_enabled == TRUE) {
		DEBUG_PRINT("The Hot-Join feature is already enabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, SET_FEATURE, HOT_JOIN, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.hot_join_enabled = TRUE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Enables the USB remote wake from regular In-Band Interrupts on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_regular_ibi_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, SET_FEATURE, REGULAR_IBI_WAKE, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Enables the USB remote wake from regular Hot-Join on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_hot_join_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, SET_FEATURE, HOT_JOIN_WAKE, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Enables the USB remote wake from an I3C Controller role request on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was set correctly, or -1 otherwise
 */
int usbi3c_enable_i3c_controller_role_request_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, SET_FEATURE, I3C_CONTROLLER_ROLE_REQUEST_WAKE, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Disables the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_i3c_bus(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, I3C_BUS, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Disables the I3C Controller role handoff.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_i3c_controller_role_handoff(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.handoff_controller_role == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handing off the I3C Controller role\n");
		return -1;
	}
	if (usbi3c_dev->device_info->device_state.handoff_controller_role_enabled == FALSE) {
		DEBUG_PRINT("The I3C Controller Role Handoff feature is already disabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, I3C_CONTROLLER_ROLE_HANDOFF, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.handoff_controller_role_enabled = FALSE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Disables all regular In-Band Interrupts on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_regular_ibi(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.in_band_interrupt_capability == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handling IBIs\n");
		return -1;
	}
	if (usbi3c_dev->device_info->device_state.in_band_interrupt_enabled == FALSE) {
		DEBUG_PRINT("The In-Band Interrupts feature is already disabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, REGULAR_IBI, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.in_band_interrupt_enabled = FALSE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Disables the Hot-Join feature on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_hot_join(struct usbi3c_device *usbi3c_dev)
{
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info->capabilities.hot_join_capability == FALSE) {
		DEBUG_PRINT("This I3C Device is not capable of handling Hot-Join\n");
		return -1;
	}
	if (usbi3c_dev->device_info->device_state.hot_join_enabled == FALSE) {
		DEBUG_PRINT("The Hot-Join feature is already disabled in this I3C device\n");
		return 0;
	}

	ret = usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, HOT_JOIN, 0);
	if (ret == 0) {
		usbi3c_dev->device_info->device_state.hot_join_enabled = FALSE;
	}

	return ret;
}

/**
 * @ingroup bus_configuration
 * @brief Disables the USB remote wake from regular In-Band Interrupts on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_regular_ibi_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, REGULAR_IBI_WAKE, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Disables the USB remote wake from regular Hot-Join on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_hot_join_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, HOT_JOIN_WAKE, 0);
}

/**
 * @ingroup bus_configuration
 * @brief Disables the USB remote wake from an I3C Controller role request on the I3C Bus.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_disable_i3c_controller_role_request_wake(struct usbi3c_device *usbi3c_dev)
{
	return usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, I3C_CONTROLLER_ROLE_REQUEST_WAKE, 0);
}

/**
 * @ingroup error_handling
 * @brief Forces all I3C Target devices to exit HDR Mode (for recovery).
 *
 * @note: Exiting HDR Mode is an important step to recovering I3C Target devices that
 * might have detected certain errors on the I3C Bus.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the feature was disabled correctly, or -1 otherwise
 */
int usbi3c_exit_hdr_mode_for_recovery(struct usbi3c_device *usbi3c_dev)
{
	struct bulk_requests *regular_requests = NULL;
	time_t initial_time;
	const int TIMEOUT = 60; // in seconds

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->request_tracker == NULL || usbi3c_dev->request_tracker->regular_requests == NULL) {
		DEBUG_PRINT("Missing regular request tracker, aborting...\n");
		return -1;
	}
	regular_requests = usbi3c_dev->request_tracker->regular_requests;

	/* if there are any outstanding bulk requests, we should wait until they
	 * finish before forcing i3c devices to exit HDR mode */
	initial_time = time(NULL);
	while (TRUE) {
		/* check to see if the timeout has not been reached */
		if (time(NULL) > initial_time + TIMEOUT) {
			DEBUG_PRINT("Timeout waiting for outstanding bulk requests to complete, aborting...\n");
			return -1;
		}

		pthread_mutex_lock(regular_requests->mutex);
		if (regular_requests->requests == NULL) {
			pthread_mutex_unlock(regular_requests->mutex);
			break;
		}
		pthread_mutex_unlock(regular_requests->mutex);

		sleep(1);
	}

	return usbi3c_set_feature(usbi3c_dev, CLEAR_FEATURE, HDR_MODE_EXIT_RECOVERY, USBI3C_BROADCAST_ADDRESS);
}

/**
 * @ingroup bus_info
 * @brief Get the list of device addresses from a target device table.
 *
 * @note An empty target device table is created during @lib_name initialization
 * using usbi3c_init(), but the table is acquired during I3C bus initialization
 * using usbi3c_initialize_device().
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[out] address_list allocated address list
 * @return list length if successful, or -1 otherwise
 */
int usbi3c_get_address_list(struct usbi3c_device *usbi3c_dev, uint8_t **address_list)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->target_device_table == NULL) {
		DEBUG_PRINT("The target device table has not been initialized, aborting...\n");
		return -1;
	}

	return table_address_list(usbi3c_dev->target_device_table, address_list);
}

/**
 * @ingroup bus_configuration
 * @brief Sets the configurable parameters of one target device.
 *
 * These are the bit-wise configurable parameters within config:
 * - IBI Timestamp (IBIT): enables or disables time-stamping of IBIs from this target device
 * - Controller Role Request (CRR): configures if the controller accepts controller role
 * requests from this target device
 * - Target Interrupt Request (TIR): configures if the controller accepts interrupts from
 * this target device
 *
 * This request is applicable when the I3C device is the active I3C controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address the address of the target device to have its configuration set
 * @param[in] config the configuration value for IBIT, CRR and TIR (only the 3 LSB are used)
 * @return 0 if the device configuration was set correctly, or -1 otherwise
 */
int usbi3c_set_target_device_config(struct usbi3c_device *usbi3c_dev, uint8_t address, uint8_t config)
{
	struct target_device *device = NULL;
	uint8_t *buffer = NULL;
	uint16_t buffer_size = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == FALSE) {
		DEBUG_PRINT("The I3C device is not the active I3C controller\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	buffer_size = device_create_set_configuration_buffer(address, config, device->device_data.max_ibi_payload_size, &buffer);
	if (usb_output_control_transfer(usbi3c_dev->usb_dev, SET_TARGET_DEVICE_CONFIG, 0, USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, buffer, buffer_size) < 0) {
		FREE(buffer);
		return -1;
	}
	device->device_data.ibi_timestamp = (config >> 2) & 0x1;
	device->device_data.controller_role_request = (config >> 1) & 0x1;
	device->device_data.target_interrupt_request = config & 0x1;

	FREE(buffer);

	return 0;
}

/**
 * @ingroup bus_configuration
 * @brief Gets the configurable parameters of one target device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address the address of the target device to have its configuration gotten
 * @param[out] config the configuration set for IBIT, CRR and TIR (only the 3 LSB are used)
 * @return 0 if the configuration is retrieved successfully, or -1 otherwise
 */
int usbi3c_get_target_device_config(struct usbi3c_device *usbi3c_dev, uint8_t address, uint8_t *config)
{
	struct target_device *device = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (config == NULL) {
		DEBUG_PRINT("A required argument is missing, aborting...\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	*config = (device->device_data.ibi_timestamp << 2) | (device->device_data.controller_role_request << 1) | device->device_data.target_interrupt_request;

	return 0;
}

/**
 * @ingroup bus_configuration
 * @brief Set USB transaction timeout.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] timeout the maximum time in milliseconds to wait for a transaction
 * @return previous timeout value
 */
unsigned int usbi3c_set_timeout(struct usbi3c_device *usbi3c_dev, unsigned int timeout)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return 0;
	}
	return usb_set_timeout(usbi3c_dev->usb_dev, timeout);
}

/**
 * @ingroup bus_configuration
 * @brief Gets USB transaction timeout.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[out] timeout the maximum time in milliseconds set to wait for a transaction
 * @return 0 if the value was retrieved successfully, or -1 otherwise
 */
int usbi3c_get_timeout(struct usbi3c_device *usbi3c_dev, unsigned int *timeout)
{
	int ret = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (timeout == NULL) {
		DEBUG_PRINT("A required argument is missing, aborting...\n");
		return -1;
	}

	ret = usb_get_timeout(usbi3c_dev->usb_dev);
	if (ret < 0) {
		*timeout = 0;
		return -1;
	}
	*timeout = ret;

	return 0;
}

/**
 * @ingroup bus_configuration
 * @brief Sets the target device max ibi payload of one target device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address the address of the target device to have its configuration set
 * @param[in] max_payload the max ibi payload to set
 * @return 0 if the device max ibi payload was set correctly, or -1 otherwise
 */
int usbi3c_set_target_device_max_ibi_payload(struct usbi3c_device *usbi3c_dev, uint8_t address, uint32_t max_payload)
{
	struct target_device *device = NULL;
	uint8_t *buffer = NULL;
	uint16_t buffer_size = 0;
	uint8_t config = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == FALSE) {
		DEBUG_PRINT("The I3C device is not the active I3C controller\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	/* get the current config values and re-send those so we keep the current config */
	config = device->device_data.target_interrupt_request;
	config = config | (device->device_data.controller_role_request << 1);
	config = config | (device->device_data.ibi_timestamp << 2);

	buffer_size = device_create_set_configuration_buffer(address, config, max_payload, &buffer);
	if (usb_output_control_transfer(usbi3c_dev->usb_dev, SET_TARGET_DEVICE_CONFIG, 0, USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, buffer, buffer_size) < 0) {
		FREE(buffer);
		return -1;
	}
	device->device_data.max_ibi_payload_size = max_payload;

	FREE(buffer);

	return 0;
}

/**
 * @ingroup bus_configuration
 * @brief Gets the target device max ibi payload of one target device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address the address of the target device to have its configuration retrieved
 * @param[out] max_payload the max ibi payload set
 * @return 0 if the device max ibi payload was retrieved correctly, or -1 otherwise
 */
int usbi3c_get_target_device_max_ibi_payload(struct usbi3c_device *usbi3c_dev, uint8_t address, uint32_t *max_payload)
{
	struct target_device *device = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (max_payload == NULL) {
		DEBUG_PRINT("A required argument is missing, aborting...\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	*max_payload = device->device_data.max_ibi_payload_size;

	return 0;
}

/**
 * @brief Changes the previously assigned dynamic address of an I3C target device.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address 7 bits of target device address (bits 0 to 6) and bit 7 set to zero
 * @param[in] new_address new device address
 * @return 0 if the request to change the device address was submitted correctly, or -1 otherwise
 */
static int usbi3c_change_dynamic_address(struct usbi3c_device *usbi3c_dev, uint8_t address, uint8_t new_address)
{
	struct target_device *device = NULL;
	uint16_t buffer_size = 0;
	uint8_t *buffer = NULL;
	int res = -1;

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	if (table_get_device(usbi3c_dev->target_device_table, new_address) != NULL) {
		DEBUG_PRINT("New address %x is already being used by another device\n", address);
		return -1;
	}

	buffer_size = device_create_address_change_buffer(device, address, new_address, &buffer);
	res = usb_output_control_transfer(usbi3c_dev->usb_dev, CHANGE_DYNAMIC_ADDRESS, 0, USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, buffer, buffer_size);
	FREE(buffer);

	return res;
}

/**
 * @ingroup bus_configuration
 * @brief Changes the previously assigned dynamic address of an I3C target device.
 *
 * This function changes the dynamic address of an I3C device and updates the target
 * device table to reflect the change. This function cannot be used to change static
 * addresses.
 *
 * @note: This request is applicable when the I3C Device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] current_address the current dynamic address of the I3C device
 * @param[in] new_address the new address to be assigned to the I3C device
 * @param[in] on_address_change_cb a callback function that will be executed when the address change request is processed
 * @param[in] data the user data to be shared with the on_address_change_cb callback function
 * @return 0 if the request to change the device address was submitted correctly, or -1 otherwise
 */
int usbi3c_change_i3c_device_address(struct usbi3c_device *usbi3c_dev, uint8_t current_address, uint8_t new_address, on_address_change_fn on_address_change_cb, void *data)
{
	struct address_change_request *request = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == FALSE) {
		DEBUG_PRINT("The I3C device is not the active I3C controller\n");
		return -1;
	}

	/* submit the request to change the address */
	if (usbi3c_change_dynamic_address(usbi3c_dev, current_address, new_address) < 0) {
		DEBUG_PRINT("The CHANGE_DYNAMIC_ADDRESS request failed\n");
		return -1;
	}

	/* add the address change request to the tracker so we can run
	 * the callback when the address change is processed */
	request = (struct address_change_request *)malloc_or_die(sizeof(struct address_change_request));
	request->request_id = (current_address << 8) + new_address;
	request->on_address_change_cb = on_address_change_cb;
	request->user_data = data;
	pthread_mutex_lock(usbi3c_dev->target_device_table->mutex);
	usbi3c_dev->target_device_table->address_change_tracker = list_append(usbi3c_dev->target_device_table->address_change_tracker, request);
	pthread_mutex_unlock(usbi3c_dev->target_device_table->mutex);

	return 0;
}

/**
 * @ingroup bus_info
 * @brief Get the bus characteristic register (BCR) of a target device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address 7 bits of target device address (bits 0 to 6) and bit 7 set to zero
 * @return the bus characteristic register if successful, or -1 otherwise
 */
int usbi3c_get_target_BCR(struct usbi3c_device *usbi3c_dev, uint8_t address)
{
	struct target_device *device = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	return device->device_data.bus_characteristic_register;
}

/**
 * @ingroup bus_info
 * @brief Get the device characteristic register (DCR) of a target device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address 7 bits of target device address (bits 0 to 6) and bit 7 set to zero
 * @return the device characteristic register if successful, or -1 otherwise
 */
int usbi3c_get_target_DCR(struct usbi3c_device *usbi3c_dev, uint8_t address)
{
	struct target_device *device = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	return device->device_data.device_characteristic_register;
}

/**
 * @ingroup bus_info
 * @brief Returns whether the target device is an I2C or an I3C device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] address 7 bits of target device address (bits 0 to 6) and bit 7 set to zero
 * @return the device type if successful, or -1 otherwise
 */
int usbi3c_get_target_type(struct usbi3c_device *usbi3c_dev, uint8_t address)
{
	struct target_device *device = NULL;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	device = table_get_device(usbi3c_dev->target_device_table, address);
	if (device == NULL) {
		DEBUG_PRINT("Address %x not reachable\n", address);
		return -1;
	}

	return device->device_data.target_type;
}

/**
 * @ingroup error_handling
 * @brief Get the USB error number.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return the usb error number
 */
int usbi3c_get_usb_error(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL || usbi3c_dev->usb_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return 0;
	}
	return usb_get_errno(usbi3c_dev->usb_dev);
}

/**
 * @ingroup command_execution
 * @brief Sends one or many commands and their associated data.
 *
 * The commands sent by this request will be executed in strict order from first to last
 * command. The I3C function will send a bulk response transfer containing response blocks
 * for all corresponding commands indicating success/failure. After the commands have been
 * sent to the I3C device, the function will wait in blocking mode until it receives the
 * response for each one the commands sent, or until it times out.
 *
 * If a command in this request stalls on NACK during execution, @lib_name will instruct the
 * I3C device to reattempt the stalled command execution up to two times by default (the number
 * of reattempts can be changed using usbi3c_set_request_reattempt_max()). If the command keeps
 * stalling, and exceeds the number of reattempts, it will be cancelled along with all the
 * subsequent commands.
 *
 * If dependent_on_previous in this request is set, and a command in the previous request
 * stalls on NACK (if exists) and it gets cancelled because of it, it will cause all the commands
 * in this request to get cancelled too.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] dependent_on_previous indicates if these commands are dependent on the previous bulk request
 * @param[in] timeout the maximum time in seconds to wait for a response after sending the commands
 * @return the responses and data sent by the I3C function, or NULL on failure
 */
struct list *usbi3c_send_commands(struct usbi3c_device *usbi3c_dev, uint8_t dependent_on_previous, int timeout)
{
	struct usbi3c_response *response = NULL;
	struct list *commands = NULL;
	struct list *request_ids = NULL;
	struct list *responses = NULL;
	struct list *node = NULL;
	time_t current_time;
	time_t initial_time;

	/* validate data */
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return NULL;
	}
	if (usbi3c_dev->command_queue == NULL) {
		DEBUG_PRINT("The command queue is empty\n");
		return NULL;
	}
	if (dependent_on_previous != USBI3C_NOT_DEPENDENT_ON_PREVIOUS && dependent_on_previous != USBI3C_DEPENDENT_ON_PREVIOUS) {
		DEBUG_PRINT("Invalid value for dependent_on_previous, aborting...\n");
		return NULL;
	}
	commands = usbi3c_dev->command_queue;

	/* since we want to wait in blocking mode for the command responses,
	 * we need to make sure there is no callback function in the commands */
	for (node = commands; node; node = node->next) {
		struct usbi3c_command *command = (struct usbi3c_command *)node->data;

		if (command == NULL) {
			DEBUG_PRINT("A command to transfer is missing, aborting...\n");
			goto FREE_QUEUE_AND_EXIT;
		}
		command->on_response_cb = NULL;
		command->user_data = NULL;
	}

	/* send the list of dependent commands and wait until we get a response */
	request_ids = bulk_transfer_send_commands(usbi3c_dev, commands, dependent_on_previous);
	if (request_ids) {
		initial_time = time(NULL);
		/* when multiple commands are sent together, their responses are received together
		 * as well, that means we can look for the first request ID in the list only, once
		 * we receive it, we can assume we have received the rest of them too */
		while (response == NULL) {
			/* check to see if the timeout has not been reached */
			current_time = time(NULL);
			if (timeout > 0 && (current_time > initial_time + timeout)) {
				DEBUG_PRINT("Timeout waiting for responses\n");
				goto FREE_QUEUE_AND_EXIT;
			}

			/* wait until we receive an event, once we do let's check the tracker
			 * to see if it was the event we were waiting for */
			usb_wait_for_next_event(usbi3c_dev->usb_dev);
			response = bulk_transfer_search_response_in_tracker(usbi3c_dev->request_tracker->regular_requests, *(uint16_t *)request_ids->data);
		}

		/* the responses should be in the tracker, let's get them */
		responses = list_append(responses, response);
		for (node = request_ids->next; node; node = node->next) {
			uint16_t request_id = *(uint16_t *)node->data;
			response = bulk_transfer_search_response_in_tracker(usbi3c_dev->request_tracker->regular_requests, request_id);
			if (response) {
				responses = list_append(responses, response);
			} else {
				usbi3c_free_responses(&responses);
				DEBUG_PRINT("The response for one of the commands is missing, aborting...\n");
				goto FREE_QUEUE_AND_EXIT;
			}
		}
	}

	/* we can clean up the command queue now */
FREE_QUEUE_AND_EXIT:
	list_free_list_and_data(&request_ids, free);
	bulk_transfer_free_commands(&usbi3c_dev->command_queue);

	return responses;
}

/**
 * @ingroup command_execution
 * @brief Submits a list of dependent commands and their associated data for execution.
 *
 * This function is similar to usbi3c_send_commands() since both functions send list of
 * commands to the I3C function. The one difference is that usbi3c_send_commands() waits
 * in blocking mode for the responses to each one of the commands in the list. While
 * usbi3c_submit_commands() uses a user provided callback function that is included with the
 * usbi3c_commands, these callbacks will be executed when the response for each request is
 * received, allowing the program to continue doing other work while waiting for the response.
 *
 * If a command in this request stalls on NACK during execution, @lib_name will instruct the
 * I3C device to reattempt the stalled command execution up to two times by default (the number
 * of reattempts can be changed using usbi3c_set_request_reattempt_max()). If the command keeps
 * stalling, and exceeds the number of reattempts, it will be cancelled along with all the
 * subsequent commands.
 *
 * If dependent_on_previous in this request is set, and a command in the previous request
 * stalls on NACK (if exists) and it gets cancelled because of it, it will cause all the commands
 * in this request to get cancelled too.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] dependent_on_previous indicates if these commands are dependent on the previous bulk request
 * @return 0 if the commands were submitted to the I3C function successfully, or -1 otherwise
 */
int usbi3c_submit_commands(struct usbi3c_device *usbi3c_dev, uint8_t dependent_on_previous)
{
	struct list *request_ids = NULL;
	struct list *node = NULL;
	struct list *commands = NULL;
	struct usbi3c_command *command = NULL;
	int ret = -1;

	/* input validation */
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->command_queue == NULL) {
		DEBUG_PRINT("The command queue is empty\n");
		return -1;
	}
	commands = usbi3c_dev->command_queue;

	for (node = commands; node; node = node->next) {
		command = (struct usbi3c_command *)node->data;

		if (command == NULL) {
			DEBUG_PRINT("A command to transfer is missing, aborting...\n");
			goto FREE_QUEUE_AND_EXIT;
		}
		if (command->on_response_cb == NULL) {
			DEBUG_PRINT("The command is missing its callback function, aborting...\n");
			goto FREE_QUEUE_AND_EXIT;
		}
	}

	/* submit the commands for execution */
	request_ids = bulk_transfer_send_commands(usbi3c_dev, commands, dependent_on_previous);
	if (request_ids == NULL) {
		list_free_list_and_data(&request_ids, free);
		goto FREE_QUEUE_AND_EXIT;
	}
	list_free_list_and_data(&request_ids, free);
	ret = 0;

	/* we can clean up the command queue now */
FREE_QUEUE_AND_EXIT:
	bulk_transfer_free_commands(&usbi3c_dev->command_queue);

	return ret;
}

/**
 * @ingroup command_execution
 * @brief Submits a vendor specific request consisting of one vendor specified data block to the I3C function.
 *
 * The data block in the vendor specific request does not need to be manually aligned
 * by the user, if the block provided is not 32-bit aligned, it will be padded
 * automatically before is transferred.
 *
 * @note Before submitting this request, a callback function for vendor specific requests
 * has to be registered using the usbi3c_on_vendor_specific_response(). This callback will
 * be triggered whenever a response to a vendor specific request is received.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] data a buffer with the vendor specific data
 * @param[in] data_size the size of the vendor specific data to be transmitted
 * @return 0 if the request was transferred correctly, or -1 otherwise
 */
int usbi3c_submit_vendor_specific_request(struct usbi3c_device *usbi3c_dev, unsigned char *data, uint32_t data_size)
{
	int ret = 0;
	uint32_t buffer_size;
	unsigned char *buffer = NULL;

	/* validate data */
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("Missing usbi3c context\n");
		return -1;
	}
	if (data == NULL || data_size == 0) {
		DEBUG_PRINT("Missing data or incorrect data size\n");
		return -1;
	}

	if (usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb == NULL) {
		DEBUG_PRINT("Missing callback function for vendor responses\n");
		return -1;
	}

	buffer_size = bulk_transfer_create_vendor_specific_buffer(&buffer, data, data_size);
	ret = usb_output_bulk_transfer(usbi3c_dev->usb_dev, buffer, buffer_size);

	FREE(buffer);

	return ret;
}

/*
 * @brief Wrapper to bulk_transfer_free_response() to be used to free lists of commands.
 *
 * The list_free_list_and_data() function receives as one of its arguments a  function of
 * the type list_free_data_fn_t, and this has a void pointer as argument. This wrapper
 * function type casts the data pointed by the void pointer to the correct type so it can
 * be used to free a list of commands.
 *
 * @param[in] data the usbi3c_response struct to free.
 *
 * @note This function should be used as an argument of function list_free_list_and_data().
 */
static void free_response_in_list(void *data)
{
	struct usbi3c_response *response = (struct usbi3c_response *)data;

	bulk_transfer_free_response(&response);
}

/**
 * @ingroup command_execution
 * @brief Frees the memory allocated for a list of usbi3c responses.
 *
 * @param[in] responses the list of responses (struct usbi3c_response) to be freed.
 */
void usbi3c_free_responses(struct list **responses)
{
	list_free_list_and_data(responses, free_response_in_list);
}

/**
 * @ingroup bus_configuration
 * @brief Sets the I3C communication mode options.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] transfer_mode sets the transfer mode for the I3C or I2C commands
 * @param[in] transfer_rate sets the transfer rate for the selected transfer mode
 * @param[in] tm_specific_info sets transfer mode specific information
 */
void usbi3c_set_i3c_mode(struct usbi3c_device *usbi3c_dev, uint8_t transfer_mode, uint8_t transfer_rate, uint8_t tm_specific_info)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return;
	}

	usbi3c_dev->i3c_mode->transfer_mode = transfer_mode;
	usbi3c_dev->i3c_mode->transfer_rate = transfer_rate;
	usbi3c_dev->i3c_mode->tm_specific_info = tm_specific_info;
}

/**
 * @ingroup bus_configuration
 * @brief Gets the I3C communication mode options.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[out] transfer_mode gets the transfer mode for the I3C or I2C commands
 * @param[out] transfer_rate gets the transfer rate for the selected transfer mode
 * @param[out] tm_specific_info gets transfer mode specific information
 * @return 0 if the values where retrieved successfully, or -1 otherwise
 */
int usbi3c_get_i3c_mode(struct usbi3c_device *usbi3c_dev, uint8_t *transfer_mode, uint8_t *transfer_rate, uint8_t *tm_specific_info)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (transfer_mode == NULL || transfer_rate == NULL || tm_specific_info == NULL) {
		DEBUG_PRINT("A required argument is missing, aborting...\n");
		return -1;
	}

	*transfer_mode = usbi3c_dev->i3c_mode->transfer_mode;
	*transfer_rate = usbi3c_dev->i3c_mode->transfer_rate;
	*tm_specific_info = usbi3c_dev->i3c_mode->tm_specific_info;

	return 0;
}

/**
 * @ingroup bus_configuration
 * @brief Sets the maximum number of reattempts for trying to resume a stalled request before canceling it.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] reattempt_max the maximum number of reattempts to set
 */
void usbi3c_set_request_reattempt_max(struct usbi3c_device *usbi3c_dev, unsigned int reattempt_max)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return;
	}
	usbi3c_dev->request_tracker->reattempt_max = reattempt_max;
}

/**
 * @ingroup bus_configuration
 * @brief Gets the maximum number of reattempts set for trying to resume a stalled request before canceling it.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[out] reattempt_max the maximum number of reattempts set
 * @return 0 if the value was retrieved successfully, or -1 otherwise
 */
int usbi3c_get_request_reattempt_max(struct usbi3c_device *usbi3c_dev, unsigned int *reattempt_max)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (reattempt_max == NULL) {
		DEBUG_PRINT("A required argument is missing, aborting...\n");
		return -1;
	}

	*reattempt_max = usbi3c_dev->request_tracker->reattempt_max;

	return 0;
}

/**
 * @ingroup error_handling
 * @brief Function to assign callback to call on I3C bus error
 *
 *  When an I3C bus error notification is triggered the callback assigned
 *  here is going to be called, if there is no callback nothing is going
 *  to be called
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_bus_error_cb callback function to call on event
 * @param[in] data data to share with the callback function when it is called
 */
void usbi3c_on_bus_error(struct usbi3c_device *usbi3c_dev, on_bus_error_fn on_bus_error_cb, void *data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return;
	}

	usbi3c_dev->bus_error_handler.on_bus_error_cb = on_bus_error_cb;
	usbi3c_dev->bus_error_handler.data = data;
}

/**
 * @ingroup bus_configuration
 * @brief Assigns a callback function that will run after a successful hot-join operation.
 *
 * When a NOTIFICATION_ADDRESS_CHANGE_STATUS notification is received as a result
 * of a successful hot-join operation, the callback function assigned here is going
 * to be executed, if there is no callback, nothing will be called.
 *
 * @note There is no need to use this callback to update the local copy of the target
 * device table, this is automatically handled by the library.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_hotjoin_cb the callback function to run after a hot-join event
 * @param[in] data the data to share with the callback function
 */
void usbi3c_on_hotjoin(struct usbi3c_device *usbi3c_dev, on_hotjoin_fn on_hotjoin_cb, void *data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return;
	}

	table_on_insert_device(usbi3c_dev->target_device_table, on_hotjoin_cb, data);
}

/**
 * @ingroup bus_configuration
 * @brief Function to assign callback to call on IBI
 *
 * When an In Band Interrupt is raised this callback is going to be called with the
 * interrupt information
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_ibi_cb callback function to call on event
 * @param[in] data data to share with the callback function when it is called
 */
void usbi3c_on_ibi(struct usbi3c_device *usbi3c_dev, on_ibi_fn on_ibi_cb, void *data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return;
	}

	ibi_set_callback(usbi3c_dev->ibi, on_ibi_cb, data);
}

/**
 * @ingroup usbi3c_target_device
 * @brief Assigns a callback function that will run after receiving an event from the active I3C controller.
 *
 * @note This function can only be used when the I3C device is not the I3C controller.
 *
 * @note The implementation of this function is expected to change in the near future. The
 * implementation details for an I3C device with a target device role are limited in version
 * 1.0 of the "Universal Serial Bus I3C Device Class Specification", which is the specification
 * this library is based upon. Once a new revision of the specification that contains more
 * implementation details for this use case is released, the functionality in this library
 * will be updated to match the specification details. So it may change functionality considerably
 * breaking backwards compatibility. Use this function at your own risk.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_controller_event_cb the callback function to run after receiving an event
 * @param[in] data the data to share with the callback function
 * @return 0 if the callback was assigned successfully, or -1 otherwise.
 */
int usbi3c_on_controller_event(struct usbi3c_device *usbi3c_dev, on_controller_event_fn on_controller_event_cb, void *data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == TRUE) {
		DEBUG_PRINT("The I3C device is the active I3C controller not a target device, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_event_handler == NULL) {
		DEBUG_PRINT("The target device event handler is missing, aborting...\n");
		return -1;
	}

	device_add_event_callback(usbi3c_dev->device_event_handler, on_controller_event_cb, data);

	return 0;
}

/**
 * @ingroup command_execution
 * @brief Assigns a callback function that will run after a vendor specific response is received.
 *
 * When a vendor specific response is received as a result of submitting a vendor
 * specific request, the callback function assigned here is going to be executed,
 * if there is no callback, nothing will be called.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_vendor_response_cb the callback function to run after a vendor specific response is received
 * @param[in] data the data to share with the callback function
 * @return 0 if the callback was assigned correctly, or -1 otherwise
 */
int usbi3c_on_vendor_specific_response(struct usbi3c_device *usbi3c_dev, on_vendor_response_fn on_vendor_response_cb, void *data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (data != NULL && on_vendor_response_cb == NULL) {
		DEBUG_PRINT("User data was provided, but a callback function is missing, aborting...\n");
		return -1;
	}

	usbi3c_dev->request_tracker->vendor_request->on_vendor_response_cb = on_vendor_response_cb;
	usbi3c_dev->request_tracker->vendor_request->user_data = data;

	return 0;
}

/**
 * @ingroup command_execution
 * @brief Adds the CCC to the queue of commands to be transmitted to the I3C function.
 *
 * Once the desired command(s) have been queued, the commands can be transmitted by using
 * either usbi3c_send_commands() or usbi3c_submit_commands().
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] target_address the target device address
 * @param[in] command_direction indicates the READ/WRITE direction of the CCC
 * @param[in] error_handling indicates the condition for the I3C controller to abort subsequent commands
 * @param[in] ccc the Common Command Code to send
 * @param[in] data_size indicates the number of bytes of data to be transferred (optional)
 * @param[in] data the data to be transferred (optional)
 * @param[in] on_response_cb a callback function to execute when a response to the command is received (optional)
 * @param[in] user_data the data to share with the on_response_cb callback function (optional)
 * @return 0 if the command was added to the queue correctly, or -1 otherwise
 */
int usbi3c_enqueue_ccc(struct usbi3c_device *usbi3c_dev,
		       uint8_t target_address,
		       enum usbi3c_command_direction command_direction,
		       enum usbi3c_command_error_handling error_handling,
		       uint8_t ccc,
		       uint32_t data_size,
		       unsigned char *data,
		       on_response_fn on_response_cb,
		       void *user_data)
{
	const int NOT_APPLICABLE = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	return bulk_transfer_enqueue_command(&usbi3c_dev->command_queue,
					     CCC_WITHOUT_DEFINING_BYTE,
					     target_address,
					     command_direction,
					     error_handling,
					     usbi3c_dev->i3c_mode,
					     ccc,
					     NOT_APPLICABLE,
					     data,
					     data_size,
					     on_response_cb,
					     user_data);
}

/**
 * @ingroup command_execution
 * @brief Adds the CCC with defining byte to the queue of commands to be transmitted to the I3C function.
 *
 * Once the desired command(s) have been queued, the commands can be transmitted by using
 * either usbi3c_send_commands() or usbi3c_submit_commands().
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] target_address the target device address
 * @param[in] command_direction indicates the READ/WRITE direction of the CCC
 * @param[in] error_handling indicates the condition for the I3C controller to abort subsequent commands
 * @param[in] ccc the Common Command Code to send
 * @param[in] defining_byte the defining byte for the CCC
 * @param[in] data_size indicates the number of bytes of data to be transferred (optional)
 * @param[in] data the data to be transferred (optional)
 * @param[in] on_response_cb a callback function to execute when a response to the command is received (optional)
 * @param[in] user_data the data to share with the on_response_cb callback function (optional)
 * @return 0 if the command was added to the queue correctly, or -1 otherwise
 */
int usbi3c_enqueue_ccc_with_defining_byte(struct usbi3c_device *usbi3c_dev,
					  uint8_t target_address,
					  enum usbi3c_command_direction command_direction,
					  enum usbi3c_command_error_handling error_handling,
					  uint8_t ccc,
					  uint8_t defining_byte,
					  uint32_t data_size,
					  unsigned char *data,
					  on_response_fn on_response_cb,
					  void *user_data)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	return bulk_transfer_enqueue_command(&usbi3c_dev->command_queue,
					     CCC_WITH_DEFINING_BYTE,
					     target_address,
					     command_direction,
					     error_handling,
					     usbi3c_dev->i3c_mode,
					     ccc,
					     defining_byte,
					     data,
					     data_size,
					     on_response_cb,
					     user_data);
}

/**
 * @ingroup command_execution
 * @brief Adds the Read/Write command to the queue of commands to be transmitted to the I3C function.
 *
 * Once the desired command(s) have been queued, the commands can be transmitted by using
 * either usbi3c_send_commands() or usbi3c_submit_commands().
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] target_address the target device address
 * @param[in] command_direction indicates the READ/WRITE direction of the command
 * @param[in] error_handling indicates the condition for the I3C controller to abort subsequent commands
 * @param[in] data_size indicates the number of bytes of data to be read or written
 * @param[in] data the data to be transferred (required with WRITE)
 * @param[in] on_response_cb a callback function to execute when a response to the command is received (optional)
 * @param[in] user_data the data to share with the on_response_cb callback function (optional)
 * @return 0 if the command was added to the queue correctly, or -1 otherwise
 */
int usbi3c_enqueue_command(struct usbi3c_device *usbi3c_dev,
			   uint8_t target_address,
			   enum usbi3c_command_direction command_direction,
			   enum usbi3c_command_error_handling error_handling,
			   uint32_t data_size,
			   unsigned char *data,
			   on_response_fn on_response_cb,
			   void *user_data)
{
	const int NOT_APPLICABLE = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	return bulk_transfer_enqueue_command(&usbi3c_dev->command_queue,
					     REGULAR_COMMAND,
					     target_address,
					     command_direction,
					     error_handling,
					     usbi3c_dev->i3c_mode,
					     NOT_APPLICABLE,
					     NOT_APPLICABLE,
					     data,
					     data_size,
					     on_response_cb,
					     user_data);
}

/**
 * @ingroup command_execution
 * @brief Adds a Target Reset Pattern to the queue of commands to be transmitted to the I3C function.
 *
 * The Target Reset mechanism allows the controller to reset one or more selected
 * target devices, and avoid resetting any others. All targets will use the defined
 * default reset action until and unless a different reset action is configured via
 * the RSTACT CCC (Broadcast and Directed formats).
 *
 * Once the desired command(s) have been queued, the commands can be transmitted by using
 * either usbi3c_send_commands() or usbi3c_submit_commands().
 *
 * @note Only the RSTACT CCC is allowed to be in the command queue when using a target
 * reset pattern.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] on_response_cb a callback function to execute when a response to the command is received (optional)
 * @param[in] user_data the data to share with the on_response_cb callback function (optional)
 * @return 0 if the command was added to the queue correctly, or -1 otherwise
 */
int usbi3c_enqueue_target_reset_pattern(struct usbi3c_device *usbi3c_dev, on_response_fn on_response_cb, void *user_data)
{
	struct usbi3c_command *command = NULL;
	struct list *node = NULL;
	const int BROADCAST_RSTACT = 0x2A;
	const int DIRECT_RSTACT = 0x9A;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	/* if a Bulk request transfer contains a Command Descriptor structure
	 * for a Target Reset Pattern, then it is recommended that such Bulk
	 * requests only contain Command Descriptors that are Target Resets
	 * Patterns and/or Command Descriptors for the RSTACT CCC with Defining
	 * Byte, so we need to check every command already in the queue to
	 * assure compliance */
	for (node = usbi3c_dev->command_queue; node; node = node->next) {
		command = (struct usbi3c_command *)node->data;
		struct command_descriptor *desc = command->command_descriptor;

		if (desc->command_type == TARGET_RESET_PATTERN) {
			/* this is valid, it is another Target Reset Pattern */
			continue;
		}
		if (desc->command_type == CCC_WITH_DEFINING_BYTE) {
			if (desc->common_command_code == BROADCAST_RSTACT || desc->common_command_code == DIRECT_RSTACT) {
				if (desc->error_handling == USBI3C_TERMINATE_ON_ANY_ERROR) {
					/* this is valid, it is a RSTACT (Target Reset Action) CCC
					 * with error handling */
					continue;
				} else {
					DEBUG_PRINT("A RSTACT CCC was found in the queue, but its error handling is not set to USBI3C_TERMINATE_ON_ANY_ERROR\n");
				}
			}
		}

		/* the command in the queue is incompatible with a reset */
		DEBUG_PRINT("There are commands in the queue that are not recommended to run along with a reset pattern, aborting...\n");
		return -1;
	}

	/* now that we know the queue has only compliant commands we can enqueue the reset pattern request */
	command = bulk_transfer_alloc_command();
	/* a newly allocated command has by default all descriptor values set to 0,
	 * so we only need to set the command_type there for the target reset pattern */
	command->command_descriptor->command_type = TARGET_RESET_PATTERN;
	command->on_response_cb = on_response_cb;
	command->user_data = user_data;
	command->data = NULL;

	usbi3c_dev->command_queue = list_append(usbi3c_dev->command_queue, command);

	return 0;
}

/**
 * @ingroup usbi3c_target_device
 * @brief Sends a request to the active I3C controller for the I3C Controller role.
 *
 * @note: This request is applicable when the I3C Device has an I3C Target Device capable of I3C secondary controller role.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 0 if the request is sent correctly, or -1 otherwise
 */
int usbi3c_request_i3c_controller_role(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	if (usbi3c_get_device_role(usbi3c_dev) != USBI3C_TARGET_DEVICE_SECONDARY_CONTROLLER_ROLE) {
		DEBUG_PRINT("The I3C device is not an I3C Device capable of Secondary Controller...\n");
		return -1;
	}
	if (usbi3c_device_is_active_controller(usbi3c_dev) == TRUE) {
		DEBUG_PRINT("The I3C device is already the active I3C controller, aborting...\n");
		return -1;
	}

	/* the I3C Secondary Controller shall issue its own Dynamic Address on the I3C
	 * Bus, followed by the RnW bit of 0 to request to become Active Controller */
	return device_send_request_to_i3c_controller(usbi3c_dev, usbi3c_dev->device_info->address, USBI3C_WRITE);
}

/**
 * @ingroup bus_info
 * @brief Gets the role of the I3C device.
 *
 * Refer to enum usbi3c_device_role for the possible role values.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return a value that represents the device role, or -1 on failure
 */
enum usbi3c_device_role usbi3c_get_device_role(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	return usbi3c_dev->device_info->device_role;
}

/**
 * @ingroup bus_info
 * @brief Indicates whether the I3C device is the active I3C controller or not.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return 1 if the I3C device is the active I3C controller, 0 if it is not, or -1 on failure
 */
int usbi3c_device_is_active_controller(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}
	return usbi3c_dev->device_info->device_state.active_i3c_controller;
}

/**
 * @ingroup usbi3c_target_device
 * @brief Adds a device to the target device table.
 *
 * This function must be used to manually add I2C or I3C devices to the target device table
 * when the I3C controller is not aware of target devices.
 * I2C devices require to have a static address.
 * I3C devices require to have a provisioned id unless they have a static address.
 * @note Assigning a value to dynamic_address has no effect since the dynamic address is
 * assigned by the I3C controller.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] device the I3C or I2C device to add to the table
 * @return 0 if the device was added successfully, or -1 otherwise
 */
int usbi3c_add_device_to_table(struct usbi3c_device *usbi3c_dev, struct usbi3c_target_device device)
{
	struct target_device *target_device = NULL;
	int enable_events;
	int ret = -1;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	/* validate there are no conflicts in device attributes */
	if (device.type == USBI3C_I2C_DEVICE) {
		if (device.static_address == 0) {
			DEBUG_PRINT("I2C devices require the static address to be specified\n");
			return -1;
		}
	} else if (device.type == USBI3C_I3C_DEVICE) {
		if (device.static_address == 0 && device.provisioned_id == 0) {
			DEBUG_PRINT("I3C devices require the PID to be specified unless they have a static address\n");
			return -1;
		}
		if (device.static_address == 0 && device.assignment_from_static_address != USBI3C_DEVICE_HAS_NO_STATIC_ADDRESS) {
			DEBUG_PRINT("static_address and assignment_from_static_address have conflicting values\n");
			return -1;
		}
	}

	/* if an address was provided, let's make sure is not already taken */
	if (device.static_address != 0) {
		if (table_get_device(usbi3c_dev->target_device_table, device.static_address) != NULL) {
			DEBUG_PRINT("Address %d is already being used by another device in the table\n", device.static_address);
			return -1;
		}
	} else {
		/* a static address was not provided, the pid should be unique then */
		if (table_get_device_by_pid(usbi3c_dev->target_device_table, device.provisioned_id) != NULL) {
			DEBUG_PRINT("PID %ld is already being used by another device in the table\n", device.provisioned_id);
			return -1;
		}
	}

	target_device = (struct target_device *)malloc_or_die(sizeof(struct target_device));

	target_device->device_data.target_type = device.type;
	target_device->target_address = device.static_address;
	target_device->pid_hi = device.provisioned_id >> 16;
	target_device->pid_lo = device.provisioned_id & 0x00000000FFFF;

	if (device.type == USBI3C_I3C_DEVICE) {
		target_device->device_data.asa = device.assignment_from_static_address;
		target_device->device_data.daa = device.dynamic_address_assignment_enabled;
		target_device->device_data.target_interrupt_request = device.target_interrupt_request_enabled ? 0 : 1;
		target_device->device_data.controller_role_request = device.controller_role_request_enabled ? 0 : 1;
		target_device->device_data.ibi_timestamp = device.ibi_timestamp_enabled;
		target_device->device_data.max_ibi_payload_size = device.max_ibi_payload_size;
		target_device->device_data.valid_pid = device.provisioned_id != 0 ? TRUE : FALSE;
	} else {
		target_device->device_data.asa = 0;
		target_device->device_data.daa = 0;
		target_device->device_data.target_interrupt_request = 0;
		target_device->device_data.controller_role_request = 0;
		target_device->device_data.ibi_timestamp = 0;
		target_device->device_data.max_ibi_payload_size = 0;
	}

	/* momentarily disable the on_insert events since we are adding
	 * the device manually */
	enable_events = usbi3c_dev->target_device_table->enable_events;
	usbi3c_dev->target_device_table->enable_events = FALSE;

	ret = table_insert_device(usbi3c_dev->target_device_table, target_device);

	usbi3c_dev->target_device_table->enable_events = enable_events;

	return ret;
}

/**
 * @ingroup bus_info
 * @brief Gets the address of the I3C device.
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @return the dynamic address of the device, or -1 on failure
 */
int usbi3c_get_device_address(struct usbi3c_device *usbi3c_dev)
{
	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (usbi3c_dev->device_info == NULL) {
		DEBUG_PRINT("The device capabilities are unknown, aborting...\n");
		return -1;
	}

	return usbi3c_dev->device_info->address;
}

/**
 * @ingroup usbi3c_target_device
 * @brief Gets the devices in the target device table.
 *
 * These devices represent the I3C/I2C devices in the I3C bus.
 * The target device table can be freed using usbi3c_free_target_device_table()
 * when no longer needed.
 * @note: This function is applicable when the usbi3c_dev device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device (I3C controller)
 * @param[out] devices the list of devices in the table
 * @return the number of devices in the table if successful, or -1 otherwise
 */
int usbi3c_get_target_device_table(struct usbi3c_device *usbi3c_dev, struct usbi3c_target_device ***devices)
{
	struct usbi3c_target_device **pub_devices = NULL;
	struct usbi3c_target_device *pub_device = NULL;
	struct target_device *target_device = NULL;
	struct list *target_devices = NULL;
	struct list *node = NULL;
	int number_of_devices = 0;
	int i = 0;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}
	if (devices == NULL) {
		DEBUG_PRINT("The target devices list is missing, aborting...\n");
		return -1;
	}

	target_devices = table_get_devices(usbi3c_dev->target_device_table);
	if (target_devices == NULL) {
		/* list empty*/
		return 0;
	}

	/* allocate memory to keep the list of devices */
	for (node = target_devices; node; node = node->next) {
		number_of_devices++;
	}
	pub_devices = malloc_or_die((number_of_devices + 1) * sizeof(struct usbi3c_target_device *));

	for (node = target_devices; node; node = node->next) {
		target_device = (struct target_device *)node->data;

		pub_device = (struct usbi3c_target_device *)malloc_or_die(sizeof(struct usbi3c_target_device));
		pub_device->type = target_device->device_data.target_type;
		pub_device->static_address = target_device->device_capability.static_address;
		/* from this point on all configs are I3C specific */
		pub_device->provisioned_id = (uint64_t)target_device->pid_hi << 16 | target_device->pid_lo;
		pub_device->dynamic_address = target_device->target_address;
		pub_device->assignment_from_static_address = target_device->device_data.asa;
		pub_device->dynamic_address_assignment_enabled = target_device->device_data.daa;
		pub_device->target_interrupt_request_enabled = target_device->device_data.target_interrupt_request ? FALSE : TRUE;
		pub_device->controller_role_request_enabled = target_device->device_data.controller_role_request ? FALSE : TRUE;
		pub_device->ibi_timestamp_enabled = target_device->device_data.ibi_timestamp;
		pub_device->max_ibi_payload_size = target_device->device_data.max_ibi_payload_size;

		pub_devices[i++] = pub_device;
	}

	/* add NULL to the last element to denote the end of the list */
	pub_devices[number_of_devices] = NULL;

	*devices = pub_devices;

	return number_of_devices;
}

/**
 * @ingroup usbi3c_target_device
 * @brief Frees the list of target devices (target device table).
 *
 * @param[in] devices the list of target devices
 */
void usbi3c_free_target_device_table(struct usbi3c_target_device ***devices)
{
	if (devices == NULL || *devices == NULL) {
		return;
	}

	struct usbi3c_target_device **usbi3c_target_device = *devices;
	for (int i = 0; usbi3c_target_device[i] != NULL; i++) {
		FREE(usbi3c_target_device[i]);
	}

	FREE(usbi3c_target_device);
	*devices = NULL;
}
