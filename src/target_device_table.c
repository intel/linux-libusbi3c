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
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "target_device_table_i.h"
#include "usbi3c_i.h"

/**
 * @brief Clean up function for the target device table.
 *
 * Clean up function used to remove the "Address Change" notification from the container.
 * It also frees the memory allocated for the target device table.
 *
 * @param[in] table The target device table to clean up.
 */
void table_destroy(struct target_device_table **table)
{
	if (table == NULL || *table == NULL) {
		return;
	}
	pthread_mutex_lock((*table)->mutex);
	list_free_list_and_data(&(*table)->target_devices, free);
	table_free_address_change_request_tracker(&(*table)->address_change_tracker);
	pthread_mutex_unlock((*table)->mutex);
	pthread_mutex_destroy((*table)->mutex);
	FREE((*table)->mutex);
	FREE(*table);
}

/**
 * @brief Compares the address change request ids provided.
 *
 * @param[in] a an address change request to have its request id compared to
 * @param[in] b the request id we are comparing against
 * @return 0 if the ids are equal
 * @return -1 if a is lower than b
 * @return 1 if a is bigger than b
 */
static int compare_address_change_request_id(const void *a, const void *b)
{
	if (((struct address_change_request *)a)->request_id < *(uint16_t *)b) {
		return -1;
	} else if (((struct address_change_request *)a)->request_id > *(uint16_t *)b) {
		return 1;
	}
	/* equal */
	return 0;
}

/**
 * @brief Frees the memory allocated for an address change request data structure in a list of requests.
 *
 * @param[in] data the address_change_request struct to free
 */
static void free_address_change_request_in_list(void *data)
{
	struct address_change_request *request = (struct address_change_request *)data;

	FREE(request);
}

/**
 * @brief Empties the address change request tracker.
 *
 * @param[in] tracker the address change request tracker
 */
void table_free_address_change_request_tracker(struct list **tracker)
{
	list_free_list_and_data(tracker, free_address_change_request_in_list);
}

/**
 * @brief Callback function that gets executed during an asynchronous address change result request.
 *
 * @param[in] user_context the user context which contains the target device table
 * @param[in] buffer the buffer containing the Address Change Result Data Structure
 * @param[in] buffer_size the size of the data in the buffer
 */
static void get_address_change_result_async(void *user_context, unsigned char *buffer, uint16_t buffer_size)
{
	struct target_device_table *table = (struct target_device_table *)user_context;
	struct address_change_request *request = NULL;
	struct list *node = NULL;

	for (int i = 0; i < GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER(buffer)->numentries; i++) {
		uint8_t old_addr = GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_N(buffer, i)->current_address;
		uint8_t new_addr = GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_N(buffer, i)->new_address;
		uint8_t request_status = GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_N(buffer, i)->status;

		if (request_status == 0) {
			/* The address change was successful, update the device table with the new address */
			if (table_change_device_address(table, old_addr, new_addr) < 0) {
				DEBUG_PRINT("Fail changing device address from %d to %d\n", old_addr, new_addr);
			}
		} else {
			DEBUG_PRINT("The I3C function reported that the address change failed from %d to %d\n", old_addr, new_addr);
		}

		/* execute the user's callback so they know the address change request was processed */
		uint16_t request_id = (old_addr << 8) + new_addr;
		node = list_search_node(table->address_change_tracker, &request_id, compare_address_change_request_id);
		if (node == NULL) {
			DEBUG_PRINT("No address change request was found that matches old address: %d, new address: %d in the tracker\n", old_addr, new_addr);
			continue;
		}
		request = (struct address_change_request *)node->data;
		if (request->on_address_change_cb) {
			request->on_address_change_cb(old_addr, new_addr, request_status, request->user_data);
			/* the callback was executed, we no longer need to track this request */
			pthread_mutex_lock(table->mutex);
			table->address_change_tracker = list_free_node(table->address_change_tracker, node, free_address_change_request_in_list);
			pthread_mutex_unlock(table->mutex);
		}
	}
}

/**
 * @brief Updates the local target device table from a GET_TARGET_DEVICE_TABLE request buffer.
 *
 * @note This function is asynchronous and expected to be used as a callback for an async
 * GET_TARGET_DEVICE_TABLE request.
 *
 * @param[in] user_context the context that contains the target device table
 * @param[in] buffer the GET_TARGET_DEVICE_TABLE request buffer
 * @param[in] buffer_size the size of the buffer
 */
static void update_target_device_info_async(void *user_context, unsigned char *buffer, uint16_t buffer_size)
{
	struct target_device_table *table = (struct target_device_table *)user_context;
	table_fill_from_device_table_buffer(table, buffer, GET_TARGET_DEVICE_TABLE_HEADER(buffer)->table_size);
}

/**
 * @brief Handles the "Address Change" notification.
 *
 * This notification is sent by the I3C Function to indicate that the Dynamic Addresses
 * of one or more I3C Target devices were successfully changed. This notification is also
 * sent by the I3C Function to indicate if a Hot-Joined I3C Target device was successfully
 * assigned an address.
 *
 * @param[in] notification the notification structure to handle this kind of notifications
 * @param[in] user_data the user context which contains the target device table
 */
void target_device_table_notification_handle(struct notification *notification, void *user_data)
{
	struct target_device_table *table = (struct target_device_table *)user_data;

	if (table == NULL) {
		DEBUG_PRINT("No target device table was provided\n");
		return;
	}

	if (notification->code == HOTJOIN_ADDRESS_ASSIGNMENT_FAILED) {
		DEBUG_PRINT("There was a failure assigning an address for a Hot-Join\n");
		return;
	}

	/* The notification code will be SUCCESSFUL_ADDRESS_CHANGE if all Address Change
	 * entries were successfully changed, or FAILURE_ADDRESS_CHANGE on encountering the
	 * first error */
	if (notification->code == SOME_ADDRESS_CHANGE_FAILED || notification->code == ALL_ADDRESS_CHANGE_SUCCEEDED) {
		if (usb_input_control_transfer_async(table->usb_dev,
						     GET_ADDRESS_CHANGE_RESULT,
						     0,
						     USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
						     get_address_change_result_async,
						     table) < 0) {
			DEBUG_PRINT("There was an error submitting the GET_ADDRESS_CHANGE_RESULT request\n");
		}
	}
	if (notification->code == HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED) {
		if (usb_input_control_transfer_async(table->usb_dev,
						     GET_TARGET_DEVICE_TABLE,
						     0,
						     USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
						     update_target_device_info_async,
						     table) < 0) {
			DEBUG_PRINT("There was an error submitting the GET_TARGET_DEVICE_TABLE request\n");
		}
	}
}

/**
 * @brief Creates a target device table struct.
 *
 * @param[in] usb_dev a USB device to make control requests
 * @return the target device table struct, or NULL on failure
 */
struct target_device_table *table_init(struct usb_device *usb_dev)
{
	if (usb_dev == NULL) {
		return NULL;
	}
	struct target_device_table *table = (struct target_device_table *)malloc_or_die(sizeof(struct target_device_table));
	table->usb_dev = usb_dev;
	table->target_devices = NULL;
	table->address_change_tracker = NULL;
	table->mutex = malloc_or_die(sizeof(pthread_mutex_t));
	pthread_mutex_init(table->mutex, NULL);
	return table;
}

/**
 * @brief get available addresses in target device table.
 *
 * if list is null just return the amount of target devices
 *
 * @param[in] table target device table
 * @param[out] list list of addresses
 * @return size of the list of addresses or -1 on failure
 */
int table_address_list(struct target_device_table *table, uint8_t **list)
{
	struct list *node = NULL;
	struct target_device *device = NULL;
	uint8_t address[ADDRESS_LEN] = { 0 };
	int address_counter = 0;

	if (table == NULL) {
		return -1;
	}

	pthread_mutex_lock(table->mutex);

	node = table->target_devices;
	for (; node; node = node->next) {
		device = (struct target_device *)node->data;
		address[address_counter++] = device->target_address;
	}

	pthread_mutex_unlock(table->mutex);

	if (!address_counter) {
		return 0;
	}

	if (list != NULL) {
		*list = malloc_or_die(sizeof(uint8_t) * address_counter);
		memcpy(*list, &address[0], sizeof(uint8_t) * address_counter);
	}

	return address_counter;
}

/**
 * @brief Compares the address of two target devices provided.
 *
 * @param[in] a the address that is being compared
 * @param[in] b the address to compare against
 * @return 0 if the addresses are equal
 * @return -1 if a is lower than b
 * @return 1 if a is bigger than b
 */
static int compare_device_address(const void *a, const void *b)
{
	if (((struct target_device *)a)->target_address < *(uint8_t *)b) {
		return -1;
	} else if (((struct target_device *)a)->target_address > *(uint8_t *)b) {
		return 1;
	}
	/* equal */
	return 0;
}

/**
 * @brief Compares the provisioned ID of two target devices provided.
 *
 * @param[in] a the pid that is being compared
 * @param[in] b the pid to compare against
 * @return 0 if the pid are equal
 * @return -1 if a is lower than b
 * @return 1 if a is bigger than b
 */
static int compare_device_pid(const void *a, const void *b)
{
	uint64_t pid;

	pid = ((struct target_device *)a)->pid_hi;
	pid = (pid << 16) + ((struct target_device *)a)->pid_lo;
	if (pid < *(uint64_t *)b) {
		return -1;
	} else if (pid > *(uint64_t *)b) {
		return 1;
	}
	/* equal */
	return 0;
}

/**
 * @brief Inserts a target device in the target device table.
 *
 * @param[in] table the target device table
 * @param[in] device the target device to add
 * @return 0 if everything was right, or -1 otherwise
 */
int table_insert_device(struct target_device_table *table, struct target_device *device)
{
	int ret = 0;

	if (table == NULL || device == NULL) {
		return -1;
	}

	pthread_mutex_lock(table->mutex);

	/* if the device has an address let's make sure the address in not already taken */
	if (device->target_address != 0) {
		if (list_search(table->target_devices, &device->target_address, compare_device_address) != NULL) {
			/* the address is already taken */
			ret = -1;
			goto EXIT;
		}
	}

	table->target_devices = list_append(table->target_devices, device);

	if (table->enable_events && table->on_insert_cb) {
		table->on_insert_cb(device->target_address, table->user_data);
	}

EXIT:
	pthread_mutex_unlock(table->mutex);
	return ret;
}

/**
 * @brief Changes the address of a target device.
 *
 * @param[in] table the target device table
 * @param[in] old_address the address of the device to have its address changed
 * @param[in] new_address the new address to be assigned
 * @return 0 if everything was right, or -1 otherwise
 */
int table_change_device_address(struct target_device_table *table, uint8_t old_address, uint8_t new_address)
{
	struct target_device *device = NULL;

	if (table == NULL) {
		return -1;
	}

	if (old_address == new_address) {
		return -1;
	}

	device = table_get_device(table, new_address);
	if (device != NULL) {
		/* the new address is already taken */
		return -1;
	}

	device = table_get_device(table, old_address);
	if (device == NULL) {
		/* not found */
		return -1;
	}

	pthread_mutex_lock(table->mutex);
	device->target_address = new_address;
	pthread_mutex_unlock(table->mutex);

	return 0;
}

/**
 * @brief Removes a target device from the target device table.
 *
 * @param[in] table the target device table
 * @param[in] address the address of the device to be removed
 * @return the target device that was removed from the table, or NULL on failure
 */
struct target_device *table_remove_device(struct target_device_table *table, uint8_t address)
{
	struct list *node = NULL;
	struct target_device *device = NULL;

	if (table == NULL) {
		return NULL;
	}

	pthread_mutex_lock(table->mutex);

	node = list_search_node(table->target_devices, &address, compare_device_address);
	if (node == NULL) {
		goto EXIT;
	}
	device = (struct target_device *)node->data;
	table->target_devices = list_free_node(table->target_devices, node, NULL);

EXIT:
	pthread_mutex_unlock(table->mutex);

	return device;
}

/**
 * @brief Gets a device from the target device table.
 *
 * @param[in] table the target device table
 * @param[in] address the address of the device to get
 * @return the target device, or NULL on failure or if no device with that address was found
 */
struct target_device *table_get_device(struct target_device_table *table, uint8_t address)
{
	struct target_device *device = NULL;

	if (table == NULL) {
		DEBUG_PRINT("The target device table has not been initialized, aborting...\n");
		return NULL;
	}

	pthread_mutex_lock(table->mutex);
	device = (struct target_device *)list_search(table->target_devices, &address, compare_device_address);
	pthread_mutex_unlock(table->mutex);

	return device;
}

/**
 * @brief Gets a device from the target device table using its provisioned ID.
 *
 * @param[in] table the target device table
 * @param[in] provisioned_id the provisioned ID of the device to get
 * @return the target device, or NULL on failure or if no device with that address was found
 */
struct target_device *table_get_device_by_pid(struct target_device_table *table, uint64_t pid)
{
	struct target_device *device = NULL;

	if (table == NULL) {
		DEBUG_PRINT("The target device table has not been initialized, aborting...\n");
		return NULL;
	}

	pthread_mutex_lock(table->mutex);
	device = (struct target_device *)list_search(table->target_devices, &pid, compare_device_pid);
	pthread_mutex_unlock(table->mutex);

	return device;
}

/**
 * @brief Fill the target device table from the capability buffer.
 *
 * @param[in] table the target device table
 * @param[in] buffer the capability buffer
 * @param[in] buffer_size the buffer size
 * @return 0 if the table was filled up successfully, or -1 on failure
 */
int table_fill_from_capability_buffer(struct target_device_table *table, uint8_t *buffer, const uint16_t buffer_size)
{
	if (table == NULL || buffer == NULL) {
		return -1;
	}

	if (buffer_size == 0) {
		return -1;
	}

	if (GET_CAPABILITY_HEADER(buffer)->error_code != DEVICE_CONTAINS_CAPABILITY_DATA) {
		return 0;
	}

	/* create or update the table with the info from target devices */
	uint16_t numentries = (buffer_size - CAPABILITY_DEVICES_OFFSET(buffer)) / CAPABILITY_DEVICE_SIZE;
	for (int i = 0; i < numentries; i++) {
		uint8_t address = GET_CAPABILITY_DEVICE_N(buffer, i)->address;
		struct target_device *device = table_get_device(table, address);
		if (device != NULL) {
			device_update_from_capability_entry(device, GET_CAPABILITY_DEVICE_N(buffer, i));
		} else {
			device = device_create_from_capability_entry(GET_CAPABILITY_DEVICE_N(buffer, i));
			if (table_insert_device(table, device) < 0) {
				FREE(device);
				return -1;
			}
		}
	}
	return 0;
}

/**
 * @brief Fills or updates the target device table from a GET_TARGET_DEVICE_TABLE request buffer.
 *
 * @param[in] table the target device table
 * @param[in] buffer the GET_TARGET_DEVICE_TABLE request buffer
 * @param[in] buffer_size the buffer size
 * @return 0 if the table was updated successfully, or -1 on failure
 */
int table_fill_from_device_table_buffer(struct target_device_table *table, uint8_t *buffer, const uint16_t buffer_size)
{
	if (table == NULL || buffer == NULL) {
		return -1;
	}

	if (buffer_size == 0) {
		return -1;
	}

	uint8_t numentries = (buffer_size - TARGET_DEVICE_ENTRY_OFFSET) / TARGET_DEVICE_ENTRY_SIZE;
	for (int i = 0; i < numentries; i++) {
		uint8_t address = GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, i)->address;
		struct target_device *device = table_get_device(table, address);
		if (device != NULL) {
			device_update_from_device_table_entry(device, GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, i));
		} else {
			device = device_create_from_device_table_entry(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, i));
			if (table_insert_device(table, device) < 0) {
				FREE(device);
				return -1;
			}
		}
	}
	return 0;
}

/**
 * @brief Creates a target device table buffer that can be used in class-specific requests.
 *
 * The INITIALIZE_I3C_BUS request uses this buffer to send the target device table to
 * the I3C controller when it requires it.
 *
 * @param[in] table the target device table
 * @param[out] buffer the created buffer
 * @return the buffer length or -1 on failure
 */
int table_create_device_table_buffer(struct target_device_table *table, uint8_t **buffer)
{
	struct target_device *device = NULL;
	struct list *node = NULL;
	int numentries = 0;
	int i = 0;

	if (table == NULL || buffer == NULL) {
		return -1;
	}

	pthread_mutex_lock(table->mutex);

	numentries = list_len(table->target_devices);
	int size = TARGET_DEVICE_HEADER_SIZE + (TARGET_DEVICE_ENTRY_SIZE * numentries);
	*buffer = malloc_or_die(size);

	GET_TARGET_DEVICE_TABLE_HEADER(*buffer)->table_size = size;

	node = table->target_devices;
	for (; node; node = node->next) {
		device = (struct target_device *)node->data;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->address = device->target_address;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->target_interrupt_request = device->device_data.target_interrupt_request;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->controller_role_request = device->device_data.controller_role_request;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->ibi_timestamp = device->device_data.ibi_timestamp;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->asa = device->device_data.asa;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->daa = device->device_data.daa;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->change_flags = device->device_data.change_flags;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->target_type = device->device_data.target_type;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->pending_read_capability = device->device_data.pending_read_capability;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->valid_pid = device->device_data.valid_pid;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->max_ibi_payload_size = device->device_data.max_ibi_payload_size;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->bcr = device->device_data.bus_characteristic_register;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->dcr = device->device_data.device_characteristic_register;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->pid_lo = device->pid_lo;
		GET_TARGET_DEVICE_TABLE_ENTRY_N(*buffer, i)->pid_hi = device->pid_hi;
		i++;
	}

	pthread_mutex_unlock(table->mutex);

	return size;
}

/**
 * @brief Creates a target device configuration buffer that can be used in class-specific requests.
 *
 * The SET_TARGET_DEVICE_CONFIG request uses this buffer to set the configurable
 * parameters of one or more target devices.
 *
 * @note This function is going to set the same configuration in all the devices
 * in the bus.
 *
 * @param[in] table the target device table
 * @param[in] config the device configuration
 * @param[in] max_ibi_payload_size the device max ibi payload size
 * @param[out] buffer the created buffer
 * @return the buffer length or -1 on failure
 */
int table_create_set_target_config_buffer(struct target_device_table *table, uint8_t config, uint32_t max_ibi_payload_size, uint8_t **buffer)
{
	struct target_device *device = NULL;
	struct list *node = NULL;
	uint16_t buffer_size = 0;
	int numentries = 0;
	int i = 0;

	if (table == NULL || buffer == NULL) {
		return -1;
	}

	pthread_mutex_lock(table->mutex);

	numentries = list_len(table->target_devices);
	buffer_size = TARGET_DEVICE_CONFIG_HEADER_SIZE + (TARGET_DEVICE_CONFIG_ENTRY_SIZE * numentries);
	*buffer = (uint8_t *)malloc_or_die(buffer_size);

	GET_TARGET_DEVICE_CONFIG_HEADER(*buffer)->config_change_command_type = CHANGE_CONFIG_COMMAND_TYPE;
	GET_TARGET_DEVICE_CONFIG_HEADER(*buffer)->numentries = numentries;

	node = table->target_devices;
	for (; node; node = node->next) {
		device = (struct target_device *)node->data;
		GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, i)->address = device->target_address;
		GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, i)->target_interrupt_request = TARGET_INTERRUPT_REQUEST_MASK & config;
		GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, i)->controller_role_request = (CONTROLLER_ROLE_REQUEST_MASK & config) >> 1;
		GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, i)->ibi_timestamp = device->device_data.ibi_timestamp;
		GET_TARGET_DEVICE_CONFIG_ENTRY_N(*buffer, i)->max_ibi_payload_size = max_ibi_payload_size;
		i++;
	}

	pthread_mutex_unlock(table->mutex);

	return buffer_size;
}

/**
 * @brief Adds a callback function that will run when a new device is added to the table.
 *
 * New devices are added to the target device table as a result of the devices joining
 * the already initialized I3C bus via a hot-join.
 *
 * @param[in] table target device table
 * @param[in] on_insert_cb callback to be called when a device is added
 * @param[in] user_data data to share with callback
 */
void table_on_insert_device(struct target_device_table *table, on_insert_fn on_insert_cb, void *user_data)
{
	if (table == NULL || on_insert_cb == NULL) {
		return;
	}
	pthread_mutex_lock(table->mutex);
	table->on_insert_cb = on_insert_cb;
	table->user_data = user_data;
	pthread_mutex_unlock(table->mutex);
}

/**
 * @brief Enable events for target device table
 *
 * @param[in] table target device table
 */
void table_enable_events(struct target_device_table *table)
{
	if (table == NULL) {
		return;
	}
	pthread_mutex_lock(table->mutex);
	table->enable_events = TRUE;
	pthread_mutex_unlock(table->mutex);
}

/**
 * @brief Gets the target device info from the I3C function and updates the local table.
 *
 * @param[in] table target device table
 * @return 0 if the local table was updated correctly, or -1 otherwise
 */
int table_update_target_device_info(struct target_device_table *table)
{
	int ret = 0;
	if (table == NULL) {
		return -1;
	}

	/* We cannot know in advanced how big the control transfer input buffer needs to be,
	 * we won't know the size of the Target Device Table data structure until after
	 * we have received the control transfer response. So to get a suitably-sized buffer for the
	 * transfer, we'll just use the maximum buffer size possible for that type of transaction. */
	uint8_t buffer[USB_MAX_CONTROL_BUFFER_SIZE] = { 0 };

	if ((ret = usb_input_control_transfer(table->usb_dev,
					      GET_TARGET_DEVICE_TABLE,
					      0,
					      USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX,
					      buffer,
					      USB_MAX_CONTROL_BUFFER_SIZE)) < 0) {
		return ret;
	}

	return table_fill_from_device_table_buffer(table, buffer, GET_TARGET_DEVICE_TABLE_HEADER(buffer)->table_size);
}

/**
 * @brief Identify the address assignment mode supported by devices in the table.
 *
 * @param[in] table the target device table
 * @param[in] support_static the number of I2C or I3C devices in the table supporting static address assignment
 * @param[in] support_dynamic the number of I3C devices in the table supporting dynamic address assignment
 * @return 0 if the devices where correctly identified, or -1 on error
 */
int table_identify_devices(struct target_device_table *table, int *support_static, int *support_dynamic)
{
	struct list *node = NULL;
	struct target_device *device = NULL;

	if (table == NULL || support_static == NULL || support_dynamic == NULL) {
		return -1;
	}

	*support_static = 0;
	*support_dynamic = 0;

	pthread_mutex_lock(table->mutex);

	node = table->target_devices;
	for (; node; node = node->next) {
		device = (struct target_device *)node->data;
		if (device->device_capability.static_address != 0) {
			/* the device has a static address, is either an I2C device,
			 * or an I2C-capable I3C device. It should support SETDASA
			 * and/or SETAASA */
			*support_static += 1;
		} else {
			/* I3C-only device that supports ENTDAA */
			if (device->pid_lo == 0 || device->pid_hi == 0) {
				/* the I3C device is missing its Provisioned ID */
				*support_static = 0;
				*support_dynamic = 0;
				pthread_mutex_unlock(table->mutex);
				DEBUG_PRINT("Found an invalid device in the table, it has no static address nor a provisioned ID\n");
				return -1;
			}
			*support_dynamic += 1;
		}
	}

	pthread_mutex_unlock(table->mutex);

	return 0;
}

/**
 * @brief Gets the devices in the target device table.
 *
 * @param[in] table the target device table
 * @return the list of devices in the table if successful, or -1 otherwise
 */
struct list *table_get_devices(struct target_device_table *table)
{
	if (table == NULL) {
		DEBUG_PRINT("The target device table is missing, aborting...\n");
		return NULL;
	}

	return table->target_devices;
}
