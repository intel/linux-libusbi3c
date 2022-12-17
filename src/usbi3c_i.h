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

#ifndef __libusbi3c_i_h__
#define __libusbi3c_i_h__

#include "common_i.h"
#include "list.h"
#include "usb_i.h"

#include "ibi_i.h"
#include "ibi_response_i.h"
#include "usbi3c.h"
#include "usbi3c_spec_i.h"

/** Reserved address used to send a request to an I3C target device
 * to hot-join an existing I3C bus */
#define HOT_JOIN_ADDRESS 0x02

#define POLLING_NOT_INITIATED 0
#define POLLING_INITIATED 1

/**
 * @brief Gets the size of a data block padded to the closest 32-bit chunk.
 *
 * The data in transfers should be 32-bit aligned, so if the data a user wants to
 * transfer isn't 32-bit aligned it needs to be padded to the closest 32-bit (4 byte)
 * chunk.
 * @param[in] size the actual size in bytes of the data to be sent or received
 * @return the size of the data block aligned to the closest 32-bit chunk
 */
static inline uint32_t get_32_bit_block_size(uint32_t size)
{
	return ((size + (DWORD_SIZE - 1)) / DWORD_SIZE) * DWORD_SIZE;
}

enum i3c_address_mode {
	I3C_CONTROLLER_DECIDED_ADDRESS_ASSIGNMENT = 0x0,
	ENTER_DYNAMIC_ADDRESS_ASSIGNMENT = 0x1,
	SET_STATIC_ADDRESS_AS_DYNAMIC_ADDRESS = 0x2
};

/**
 * @brief Enumeration of possible actions for the CANCEL_OR_RESUME_BULK_REQUEST.
 */
enum i3c_cancel_or_resume_bulk_request {
	CANCEL_BULK_REQUEST = 0x0, ///< clear the stalled command and cancel subsequent dependent commands
	RESUME_BULK_REQUEST = 0x1  ///< retry the stalled command and continue normally if successful
};

typedef uint16_t (*get_len_fn)(uint8_t *header);

/**
 * @brief Structure representing a @lib_name session.
 *
 * This struct is obtained by calling usbi3c_init().
 */
struct usbi3c_context {
	struct usb_context *usb_ctx; ///< The USB context used by this context
	int ref_count;		     ///< The number of references to this context.
};

/**
 * @brief Structure to represent an USBI3C notification
 *
 * This struct contains the type of notification and the code associated with it.
 */
struct notification {
	uint8_t type;  ///< The type of notification
	uint16_t code; ///< The code associated with the notification
};

/**
 * @brief Structure to handle an USBI3C notification
 *
 * This struct contains the notification and the callback function to be called
 * when the notification is received, along with the user data to be passed to
 * the callback function.
 */
struct notification_handler {
	void (*handle)(struct notification *notification, void *user_data); ///< handler to manage notification
	void *user_data;						    ///< user data to be passed to the handler
};

struct bus_error_handler {
	on_bus_error_fn on_bus_error_cb; ///< User provided callback to call when a bus error notification is triggered
	void *data;			 ///< Data to share with bus error callback
};

/**
 * @brief Structure representing an usbi3c device (a USB device with an I3C interface).
 *
 * This structure represent an usb device with an I3C interface.
 **/
struct usbi3c_device {
	struct usb_device *usb_dev;					  ///< The USB device used by this @lib_name session.
	struct usbi3c_context *usbi3c_ctx;				  ///< The @lib_name context used by this device.
	struct device_info *device_info;				  ///< Info about i3c device connected to usb
	struct target_device_table *target_device_table;		  ///< The target device table
	pthread_mutex_t lock;						  ///< Mutex to protect access to the device
	int bus_init_status;						  ///< I3C bus status
	struct bus_error_handler bus_error_handler;			  ///< Bus error handler
	int bus_error;							  ///< I3C bus error handler info
	struct notification_handler handlers[NOTIFICATION_HANDLERS_SIZE]; ///< notification handler table
	struct i3c_mode *i3c_mode;					  ///< Specifies the I3C communication modes
	struct list *command_queue;					  ///< Queue of commands to be sent to the I3C function
	struct request_tracker *request_tracker;			  ///< Tracks all unanswered requests sent to an I3C function
	struct ibi *ibi;						  ///< IBI handler
	struct device_event_handler *device_event_handler;		  ///< Handles events received from the active I3C controller
	int ref_count;							  ///< The number of references to this device.
};

/**
 * @brief Structure representing the capabilities of the I3C bus.
 */
struct usbi3c_bus_capabilities {
	uint8_t devices_present;
	uint8_t handoff_controller_role;
	uint8_t hot_join_capability;
	uint8_t in_band_interrupt_capability;
	uint8_t pending_read_capability;
	uint8_t self_initiated;
	uint8_t delayed_pending_read;
	uint8_t pending_read_sdr;
	uint8_t pending_read_hdr;
	uint8_t single_command_pending_read;
	uint16_t i3c_minor_ver;
	uint16_t i3c_major_ver;
	uint16_t disco_minor_ver;
	uint16_t disco_major_ver;
	uint8_t i2c_data_transfer_rates;
	uint16_t clock_frequency_i2c_udr1;
	uint16_t clock_frequency_i2c_udr2;
	uint16_t clock_frequency_i2c_udr3;
	uint8_t i3c_data_transfer_modes;
	uint8_t i3c_data_transfer_rates;
	uint16_t transfer_mode_extended_capability_length;
	uint32_t clock_frequency_i3c_udr1;
	uint32_t clock_frequency_i3c_udr2;
	uint32_t max_ibi_payload_size;
};

/**
 * @brief Structure that stores the current state of the I3C device.
 */
struct device_state {
	uint8_t active_i3c_controller;		 ///< indicates if the I3C device is the active I3C controller
	uint8_t handoff_controller_role_enabled; ///< indicates if the I3C device has the Handoff Controller Role feature enabled
	uint8_t hot_join_enabled;		 ///< indicates if the I3C device has the Hot-Join feature enabled
	uint8_t in_band_interrupt_enabled;	 ///< indicates if the I3C device has the IBIs feature enabled
};

struct device_info {
	uint8_t address;
	uint8_t device_role; ///< indicates the role of the I3C Device
	uint8_t data_type;   ///< indicates the type of data in the I3C Capability data structure
	struct usbi3c_bus_capabilities capabilities;
	struct device_state device_state;
};

/**
 * @brief A structure that describes an I3C command.
 */
struct command_descriptor {
	uint8_t command_type;	     ///< Defines the command type to be sent
	uint8_t command_direction;   ///< Indicates the direction of command
	uint8_t error_handling;	     ///< Indicates condition for the I3C controller to abort subsequent commands
	uint8_t target_address;	     ///< the target device address
	uint8_t transfer_mode;	     ///< Indicates the transfer mode for the I3C or I2C commands
	uint8_t transfer_rate;	     ///< Indicates the transfer rate for the selected transfer mode
	uint8_t tm_specific_info;    ///< Reserved for transfer mode specific information
	uint8_t defining_byte;	     ///< Contains the defining byte for the CCC (if applicable)
	uint8_t common_command_code; ///< Contains the value for CCC (if applicable)
	uint32_t data_length;	     ///< Indicates the number of bytes of data to be transferred (if any)
};

/**
 * @brief Data structure to track I3C commands sent via USB bulk request transfers.
 *
 * A bulk request transfer can contain a single independent command,
 * or a list of dependent commands.
 *
 * For the case of the single independent command, when the I3C function
 * sends the response to the command, it will send a single response
 * transfer with the details and data.
 * For the case of multiple dependent commands, the I3C function will still
 * send only a single response transfer, but this transfer will contain all
 * the individual responses to the commands.
 *
 * This structure serves three main purposes:
 * - tracks the number of commands that were sent in the request transfer
 *   so we know how many responses to parse.
 * - track the responses sent by the I3C function when it is available
 *   matching it with its particular request ID, if there is a callback
 *   assigned to that request, it passes the response to that callback and
 *   executes it.
 * - Indicates if a request is dependent on previous requests, this is used
 *   to act on this request if the previous dependent request stalls.
 */
struct regular_request {
	uint16_t request_id;		  ///< the ID of the command being tracked
	int total_commands;		  ///< the total number of commands sent to the I3C function in the same request transfer
	int dependent_on_previous;	  ///< indicates if that particular request is dependent on the correct execution of a previous command
	int reattempt_count;		  ///< number of times the request has been reattempted after stalling
	struct usbi3c_response *response; ///< a pointer to the corresponding response received from the I3C function when available
	on_response_fn on_response_cb;	  ///< callback function to execute when the response is received
	void *user_data;		  ///< user data to share with the on_response_cb callback function
};

/**
 * @brief Data structure that holds data to handle vendor specific requests.
 */
struct vendor_specific_request {
	on_vendor_response_fn on_vendor_response_cb; ///< callback function to execute when a vendor response is received
	void *user_data;			     ///< user data to share with the on_vendor_response_cb callback function
};

/**
 * @brief Data structure to track bulk requests.
 */
struct bulk_requests {
	struct list *requests;	///< A list of requests that are being tracked
	pthread_mutex_t *mutex; ///< Race condition protection to access the request tracker
};

/**
 * @brief Data structure that tracks all requests sent to an I3C function.
 */
struct request_tracker {
	struct usb_device *usb_dev;			///< the usb session
	unsigned int reattempt_max;			///< maximum number of times to reattempt a stalled request
	struct bulk_requests *regular_requests;		///< Regular request tracker
	struct ibi_response_queue *ibi_response_queue;	///< IBI response queue
	struct ibi *ibi;				///< IBI handler
	struct vendor_specific_request *vendor_request; ///< vendor request handler
};

/**
 * @brief A structure representing an I3C command along with its data.
 *
 * This structure contains all the information that conforms an I3C command.
 */
struct usbi3c_command {
	struct command_descriptor *command_descriptor; ///< It defines the characteristics of an I3C command
	unsigned char *data;			       ///< Optional data buffer to attach to a command
	on_response_fn on_response_cb;		       ///< Callback function to executed when the response is received
	void *user_data;			       ///< User data to share with the on_response_cb callback function
};

/**
 * @brief Enumeration of command types in command descriptions.
 */
enum bulk_request_command_descriptor_command_type {
	REGULAR_COMMAND = 0x0,
	CCC_WITHOUT_DEFINING_BYTE = 0x1,
	CCC_WITH_DEFINING_BYTE = 0x2,
	TARGET_RESET_PATTERN = 0X3,
};

/**
 * @brief Data structure that specifies the I3C communication mode options.
 */
struct i3c_mode {
	uint8_t transfer_mode;	  ///< Indicates the transfer mode for the I3C or I2C commands
	uint8_t transfer_rate;	  ///< Indicates the transfer rate for the selected transfer mode
	uint8_t tm_specific_info; ///< Reserved for transfer mode specific information
};

/**************************/
/* bulk transfer requests */
/**************************/
/* initialization */
struct i3c_mode *i3c_mode_init(void);
struct request_tracker *bulk_transfer_request_tracker_init(struct usb_device *usb_dev, struct ibi_response_queue *ibi_response_queue, struct ibi *ibi);
void request_tracker_destroy(struct request_tracker **request_tracker);
void stall_on_nack_handle(struct notification *notification, void *user_data);
/* buffer */
uint32_t bulk_transfer_create_vendor_specific_buffer(unsigned char **buffer, unsigned char *data, uint32_t data_size);
/* commands */
struct usbi3c_command *bulk_transfer_alloc_command(void);
int bulk_transfer_validate_command(struct usbi3c_command *command);
struct list *bulk_transfer_send_commands(struct usbi3c_device *usbi3c_dev, struct list *commands, uint8_t dependent_on_previous);
int bulk_transfer_remove_command_and_dependent(struct bulk_requests *regular_requests, uint16_t request_id);
int bulk_transfer_cancel_request_async(struct usb_device *usb_dev, struct bulk_requests *regular_requests, uint16_t request_id);
int bulk_transfer_resume_request_async(struct usb_device *usb_dev);
int bulk_transfer_enqueue_command(struct list **command_queue, uint8_t command_type, uint8_t target_address, uint8_t command_direction, uint8_t error_handling, struct i3c_mode *i3c_mode, uint8_t ccc, uint8_t defining_byte, unsigned char *data, uint32_t data_size, on_response_fn on_response_cb, void *user_data);
void bulk_transfer_free_command(struct usbi3c_command **command);
void bulk_transfer_free_commands(struct list **commands);
void bulk_transfer_free_response(struct usbi3c_response **response);
/* responses */
void bulk_transfer_get_response(void *context, unsigned char *buffer, uint32_t buffer_size);
int bulk_transfer_get_regular_response(struct bulk_requests *regular_requests, unsigned char *buffer, uint32_t buffer_size);
int bulk_transfer_get_vendor_specific_response(struct vendor_specific_request *vendor_request, unsigned char *buffer, uint32_t buffer_size);
struct usbi3c_response *bulk_transfer_search_response_in_tracker(struct bulk_requests *regular_requests, int request_id);

/* matchers */
int compare_request_id(const void *a, const void *b);

#endif // __libusbi3c_i_h__
