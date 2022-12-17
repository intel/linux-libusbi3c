/***************************************************************************
  target_device.c  - target device operations
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
#include <stdlib.h>

#include "target_device_table_i.h"
#include "usbi3c_i.h"

/**
 * @brief Data structure that represents the I3C controller event handler.
 */
struct device_event_handler {
	on_controller_event_fn on_controller_event_cb; ///< user provided callback function to call when an event is received from the I3C controller
	void *data;				       ///< the data to share with the on_controller_event_cb callback function
	pthread_mutex_t *mutex;			       ///< Race condition protection
};

/**
 * @brief Creates a target device struct from a device capability entry.
 *
 * @param[in] entry the device capability entry
 * @return the target device created, or NULL on error
 */
struct target_device *device_create_from_capability_entry(struct capability_device_entry *entry)
{
	if (entry == NULL) {
		DEBUG_PRINT("No capability entry provided\n");
		return NULL;
	}
	struct target_device *device = (struct target_device *)malloc_or_die(sizeof(struct target_device));
	device->pid_lo = entry->pid_lo;
	device->pid_hi = entry->pid_hi;
	device->target_address = entry->address;
	device_update_from_capability_entry(device, entry);
	return device;
}

/**
 * @brief Updates a target device struct from a device capability entry.
 *
 * @param[in] device the target device struct to update
 * @param[in] entry the device capability entry
 */
void device_update_from_capability_entry(struct target_device *device, struct capability_device_entry *entry)
{
	if (device == NULL || entry == NULL) {
		DEBUG_PRINT("No target device or capability entry provided\n");
		return;
	}
	device->device_capability.ibi_prioritization = entry->ibi_prioritization;
	device->device_capability.disco_minor_ver = entry->mipi_disco_minor_version;
	device->device_capability.disco_major_ver = entry->mipi_disco_major_version;
	device->device_capability.max_ibi_pending_read_size = entry->max_ibi_pending_size;
}

/**
 * @brief create target device struct from target device table entry.
 *
 * @param[in] entry the target device table entry
 * @return target device struct
 */
struct target_device *device_create_from_device_table_entry(struct target_device_table_entry *entry)
{
	if (entry == NULL) {
		DEBUG_PRINT("No target device entry provided\n");
		return NULL;
	}
	struct target_device *device = (struct target_device *)malloc_or_die(sizeof(struct target_device));
	device->pid_lo = entry->pid_lo;
	device->pid_hi = entry->pid_hi;
	device->target_address = entry->address;
	device_update_from_device_table_entry(device, entry);
	return device;
}

/**
 * @brief update target device struct from target device table entry.
 *
 * @param[in] device the target device struct to update
 * @param[in] entry the target device table entry
 */
void device_update_from_device_table_entry(struct target_device *device, struct target_device_table_entry *entry)
{
	if (device == NULL || entry == NULL) {
		DEBUG_PRINT("No target device or target device entry provided\n");
		return;
	}
	device->device_data.target_interrupt_request = entry->target_interrupt_request;
	device->device_data.controller_role_request = entry->controller_role_request;
	device->device_data.ibi_timestamp = entry->ibi_timestamp;
	device->device_data.asa = entry->asa;
	device->device_data.daa = entry->daa;
	device->device_data.change_flags = entry->change_flags;
	device->device_data.target_type = entry->target_type;
	device->device_data.pending_read_capability = entry->pending_read_capability;
	device->device_data.valid_pid = entry->valid_pid;
	device->device_data.max_ibi_payload_size = entry->max_ibi_payload_size;
	device->device_data.bus_characteristic_register = entry->bcr;
	device->device_data.device_characteristic_register = entry->dcr;
}

/**
 * @brief Creates a device configuration buffer.
 *
 * This buffer can be sent in a request to the I3C function to update the configuration
 * of a specific target device.
 *
 * @param[in] address the address of the target device to configure
 * @param[in] config the new device configuration
 * @param[in] max_ibi_payload_size the max IBI payload size to set for the given device
 * @param[out] buffer the generated buffer
 * @return the size of the generated buffer
 */
int device_create_set_configuration_buffer(uint8_t address, uint8_t config, uint32_t max_ibi_payload_size, uint8_t **buffer)
{
	uint16_t buffer_size = TARGET_DEVICE_CONFIG_HEADER_SIZE + TARGET_DEVICE_CONFIG_ENTRY_SIZE;
	*buffer = (uint8_t *)malloc_or_die(buffer_size);

	GET_TARGET_DEVICE_CONFIG_HEADER(*buffer)->config_change_command_type = CHANGE_CONFIG_COMMAND_TYPE;
	GET_TARGET_DEVICE_CONFIG_HEADER(*buffer)->numentries = 1;

	GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, 0)->address = address;
	GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, 0)->target_interrupt_request = config & 0x1;
	GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, 0)->controller_role_request = (config >> 1) & 0x1;
	GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, 0)->ibi_timestamp = (config >> 2) & 0x1;
	GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, 0)->max_ibi_payload_size = max_ibi_payload_size;

	return buffer_size;
}

/* Gets the size of an address change buffer with N entries */
static size_t get_address_change_buffer_size(int numentries)
{
	return (TARGET_DEVICE_ADDRESS_CHANGE_HEADER_SIZE +
		(TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_SIZE * numentries));
}

/**
 * @brief Creates a device address change buffer.
 *
 * This buffer contains the required data structure that needs to be sent
 * to the I3C Function in the CHANGE_DYNAMIC_ADDRESS request to change the
 * address of one target device.
 *
 * @param[in] device the I3C device that is to have its address changed
 * @param[in] address 7 bits of the current dynamic address (bits 6:0) of the Target device and 1 bit (bit 7) set to 0
 * @param[in] new_address 7 bits of the new dynamic address (bits 6:0) of the Target device and 1 bit (bit 7) set to 0
 * @param[out] buffer the generated buffer with the address change data structure
 * @return the size of the generated buffer
 */
uint16_t device_create_address_change_buffer(struct target_device *device, uint8_t address, uint8_t new_address, uint8_t **buffer)
{
	const int NUMENTRIES = 1; // we are changing the address of just one device
	size_t buffer_size = get_address_change_buffer_size(NUMENTRIES);
	*buffer = (uint8_t *)malloc_or_die(buffer_size);

	GET_TARGET_DEVICE_ADDRESS_CHANGE_HEADER(*buffer)->address_change_command_type = ADDRESS_CHANGE_COMMAND_TYPE;
	GET_TARGET_DEVICE_ADDRESS_CHANGE_HEADER(*buffer)->numentries = NUMENTRIES;

	GET_TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_N(*buffer, 0)->current_address = address;
	GET_TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_N(*buffer, 0)->new_address = new_address;
	GET_TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_N(*buffer, 0)->pid_lo = device->pid_lo;
	GET_TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_N(*buffer, 0)->pid_hi = device->pid_hi;

	return (uint16_t)buffer_size;
}

/**
 * @brief Handles the "Active I3C Controller Event" notification.
 *
 * This notification is sent by the I3C Function to indicate events from the active I3C controller
 * on I3C bus to either this I3C target device, or this I3C target device capable of secondary I3C
 * controller role.
 *
 * @param[in] notification the notification structure for "Active I3C Controller Event"
 * @param[in] user_data pointer to device_event_handler structure to be used for handling the event
 */
void device_handle_event(struct notification *notification, void *user_data)
{
	struct device_event_handler *device_event_handler = NULL;

	device_event_handler = (struct device_event_handler *)user_data;
	pthread_mutex_lock(device_event_handler->mutex);
	if (device_event_handler->on_controller_event_cb != NULL) {
		device_event_handler->on_controller_event_cb(notification->code, device_event_handler->data);
	}
	pthread_mutex_unlock(device_event_handler->mutex);
}

/**
 * @brief Clean up function for the "Active I3C Controller Event" notification handler.
 *
 * Clean up function used to remove the "Active I3C Controller" notification from
 * the container, and to free up the memory used for the handler. This function is
 * going to be called during library de-initialization.
 *
 * @param[in] device_event_handler the device event handler to clean up
 */
void device_destroy_event_handler(struct device_event_handler **device_event_handler)
{
	if (device_event_handler == NULL || *device_event_handler == NULL) {
		return;
	}
	pthread_mutex_destroy((*device_event_handler)->mutex);
	FREE((*device_event_handler)->mutex);
	FREE(*device_event_handler);
}

/**
 * @brief Assigns a callback function that will run after an "Active I3C Controller Event" notification is received.
 *
 * When an ACTIVE_I3C_CONTROLLER_EVENT notification is received as a result of the active I3C
 * controller sending an event to the target device, the callback function assigned here is going
 * to be executed, if there is no callback, nothing will be called.
 *
 * @param[in] device_event_handler the target device event handler
 * @param[in] on_controller_event_cb the callback function to execute after an I3C controller event
 * @param[in] data the data to share with the callback function
 */
void device_add_event_callback(struct device_event_handler *device_event_handler, on_controller_event_fn on_controller_event_cb, void *data)
{
	pthread_mutex_lock(device_event_handler->mutex);
	device_event_handler->on_controller_event_cb = on_controller_event_cb;
	device_event_handler->data = data;
	pthread_mutex_unlock(device_event_handler->mutex);
}

/**
 * @brief Initializes the device event handler.
 *
 * These events are sent to the device by the active I3C controller.
 * @note: This notification is applicable when the I3C Device is not in the I3C Controller role.
 *
 * @return the device event handler, or NULL on failure
 */
struct device_event_handler *device_event_handler_init(void)
{
	struct device_event_handler *device_event_handler = NULL;

	device_event_handler = (struct device_event_handler *)malloc_or_die(sizeof(struct device_event_handler));
	device_event_handler->mutex = malloc_or_die(sizeof(pthread_mutex_t));
	pthread_mutex_init(device_event_handler->mutex, NULL);
	device_event_handler->on_controller_event_cb = NULL;
	device_event_handler->data = NULL;

	return device_event_handler;
}

/**
 * @brief Sends a request using the I3C Arbitrable Address Header.
 *
 * Using the I3C Arbitrable Address Header, I3C Targets may transmit any of
 * three requests to the I3C Controller:
 * - An In-Band Interrupt: uses the Target’s dynamic address with a RnW bit of 1
 * - A Controller Role Request: uses the Target’s dynamic address with a RnW bit of 0
 * - A Hot-Join Request: uses the reserved Hot-Join Address (0x02)
 *
 * @param[in] usbi3c_dev the usbi3c device
 * @param[in] target_address the address to use for the request
 * @param[in] read_n_write the read/write bit to use for the request 1=read, 0=write
 * @return 0 if the request was sent successfully, or -1 otherwise
 */
int device_send_request_to_i3c_controller(struct usbi3c_device *usbi3c_dev, uint8_t target_address, uint8_t read_n_write)
{
	struct list *responses = NULL;
	struct usbi3c_response *response = NULL;
	int ret = -1;
	const int TIMEOUT = 60;

	if (usbi3c_dev == NULL) {
		DEBUG_PRINT("The usbi3c device is missing, aborting...\n");
		return -1;
	}

	if (read_n_write != USBI3C_READ && read_n_write != USBI3C_WRITE) {
		DEBUG_PRINT("Invalid read_n_write value\n");
		return -1;
	}

	/* requests sent with these function have to be sent alone, so let's
	 * check there are no commands in the queue already */
	if (usbi3c_dev->command_queue != NULL) {
		DEBUG_PRINT("The command queue has unsent requests\n");
		return -1;
	}

	/* check the status of the bulk response transfer polling to make
	 * sure it has been initiated, otherwise the function may hang */
	if (usb_input_bulk_transfer_polling_status(usbi3c_dev->usb_dev) == POLLING_NOT_INITIATED) {
		DEBUG_PRINT("The bulk response transfer polling hasn't been initiated\n");
		return -1;
	}

	usbi3c_enqueue_command(usbi3c_dev,
			       target_address,
			       read_n_write,
			       USBI3C_TERMINATE_ON_ANY_ERROR,
			       USBI3C_RESPONSE_HAS_NO_DATA,
			       NULL,
			       NULL,
			       NULL);

	responses = usbi3c_send_commands(usbi3c_dev, USBI3C_NOT_DEPENDENT_ON_PREVIOUS, TIMEOUT);
	if (responses == NULL) {
		return -1;
	}

	/* we are only expecting to get one response */
	response = (struct usbi3c_response *)responses->data;
	if (response->attempted != USBI3C_COMMAND_ATTEMPTED) {
		DEBUG_PRINT("Request not attempted\n");
		goto FREE_AND_EXIT;
	}

	ret = response->error_status;

FREE_AND_EXIT:
	usbi3c_free_responses(&responses);

	return ret;
}
