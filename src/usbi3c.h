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

#ifndef __libusbi3c_h__
#define __libusbi3c_h__

#include <stdint.h>
#include <sys/types.h>

#include "list.h"
#include "usbi3c_commands.h"

/* Constant value used in I3C class-specific requests that do not
 * require a specific target device address */
#define NO_ADDRESS 0

/* I3C special addresses */
#define USBI3C_BROADCAST_ADDRESS 0x7E

/** Default device class */
extern const int USBI3C_DeviceClass;

/**
 * @ingroup bus_configuration
 * @brief Enumerates the result status of an I3C target device address change request.
 */
enum usbi3c_address_change_status {
	USBI3C_ADDRESS_CHANGE_SUCCEEDED = 0x0, ///< The dynamic address of the I3C target device was changed successfully
	USBI3C_ADDRESS_CHANGE_FAILED = 0x1     ///< The dynamic address of the I3C target device failed to be changed
};

/**
 * @ingroup bus_configuration
 * @brief Definition of a callback function used after an I3C bus error.
 *
 * The callback will be executed after an "I3C Bus Error" notification has been received
 * from the I3C controller. This callback function has to be passed as an argument to the
 * usbi3c_on_bus_error() function.
 */
typedef void (*on_bus_error_fn)(uint8_t error, void *user_data);

/**
 * @ingroup usbi3c_target_device
 * @brief Enumeration of the type of events that a target device can receive from the active I3C controller.
 */
enum usbi3c_controller_event_code {
	USBI3C_RECEIVED_CCC = 0x2,	     ///< Target device received a CCC from the active I3C controller
	USBI3C_RECEIVED_READ_REQUEST = 0x3,  ///< Target device received a Read request from the active I3C controller
	USBI3C_RECEIVED_WRITE_REQUEST = 0x4, ///< Target device received a Write request from the active I3C controller
};

/**
 * @brief Definition of a callback function used after an I3C controller event.
 *
 * The callback function will be executed after receiving an event from the active I3C
 * controller. This callback function has to be passed as an argument in the
 * usbi3c_on_controller_event() function.
 */
typedef void (*on_controller_event_fn)(enum usbi3c_controller_event_code event_code, void *user_data);

/**
 * @ingroup bus_configuration
 * @brief Definition of a callback function used after a successful Hot-Join.
 *
 * The callback will be executed after an I3C device has successfully Hot-Joined the
 * I3C bus. This callback function has to be passed as an argument in the usbi3c_on_hotjoin()
 * function.
 */
typedef void (*on_hotjoin_fn)(uint8_t address, void *user_data);

struct usbi3c_ibi;

/**
 * @brief function to be called when an IBI is completed
 *
 * param[in] report the reason why this IBI was triggered
 * param[in] descriptor structure describing the completed IBI
 * param[in] data data associated with this IBI if exists if not NULL
 * param[in] size the size of the data associated with this IBI if it exists if not 0
 * param[in] user_data that from user to share with IBI callback
 */
typedef void (*on_ibi_fn)(uint8_t report,
			  struct usbi3c_ibi *descriptor,
			  uint8_t *data,
			  size_t size,
			  void *user_data);

/**
 * @ingroup command_execution
 * @brief Definition of a callback function used after a vendor specific response is received.
 *
 * The callback will be executed after a vendor specific response has been received from the I3C
 * function. This callback function has to be passed as an argument in the
 * usbi3c_on_vendor_specific_response() function.
 */
typedef void (*on_vendor_response_fn)(int response_data_size, void *response_data, void *user_data);

#define IBI_DESCRIPTOR_TYPE_REGULAR 0
#define IBI_DESCRIPTOR_TYPE_NON_REGULAR 1

/**
 * @brief A structure that describes an IBI response.
 */
struct usbi3c_ibi {
	uint8_t address : 7;	   ///< Address that issued an IBI
	uint8_t R_W : 1;	   ///< Read/Write flag 0 Write, 1 Read
	uint8_t ibi_status : 1;	   ///< IBI status 0 acknowledged, 1 not acknowledged
	uint8_t error : 1;	   ///< If 1 this IBI was caused due to an error in execution of a command
	uint8_t ibi_timestamp : 1; ///< 1 if the IBI is timestamped
	uint8_t ibi_type : 1;	   ///< 0 if is a regular IBI, 1 if it is scheduled or from a secondary controller
	union {
		uint8_t MDB; ///< Mandatory data byte
		struct {
			uint8_t specific_interrupt_id : 5; ///< Specific interrupt id to identify what has happened
			uint8_t interrupt_group_id : 3;	   ///< Interrupt group identifier that this IBI belongs
		} MDB_specific;
	};
};

/**
 * @ingroup bus_configuration
 * @brief Definition of a callback function for an I3C address change request.
 *
 * The callback will be executed when a request to change an I3C device address is processed,
 * and it has to be passed as an argument when submitting the request in the usbi3c_change_i3c_device_address()
 * function.
 */
typedef void (*on_address_change_fn)(uint8_t old_address, uint8_t new_address, enum usbi3c_address_change_status request_status, void *user_data);

/**
 * @brief A structure storing @lib_name version information.
 */
struct usbi3c_version_info {
	int major;		  ///< Library major version
	int minor;		  ///< Library minor version
	int patch;		  ///< May be used for hot-fixes
	const char *version_str;  ///< Library version as (static) string
	const char *snapshot_str; ///< Git snapshot version if known. Otherwise "unknown" or empty string
};

/**
 * @ingroup command_execution
 * @brief A structure representing data from a bulk response sent by an I3C function.
 *
 * This structure is sent as a bulk response to the host’s bulk request with one or more
 * commands, or with a vendor specific response. If the data structure is from a regular
 * request, it represents only one command regardless of how many commands were included
 * in the request.
 *
 * @note This is structure is populated by bulk_transfer_get_response()
 */
struct usbi3c_response {
	uint8_t attempted;    ///< Indicates if the command in the corresponding request was attempted
	uint8_t error_status; ///< Indicates the status of the processed command
	uint8_t has_data;     ///< Indicates if the response block has data appended
	uint32_t data_length; ///< Indicates the number of bytes of appended data (if any)
	unsigned char *data;  ///< Contains the data associated with the response
};

/**
 * @ingroup command_execution
 * @brief Definition of a callback function that can be provided when submitting I3C commands.
 *
 * This callback will be executed when a response to the corresponding command is available.
 * Note that this callback has to be added to the usbi3c_command that will be submitted, and
 * this usbi3c_command has to be submitted using usbi3c_submit_commands().
 */
typedef int (*on_response_fn)(struct usbi3c_response *response, void *user_data);

/**
 * @ingroup bus_configuration
 * @brief Enumeration of target device types.
 */
enum usbi3c_target_device_type {
	USBI3C_I3C_DEVICE = 0,
	USBI3C_I2C_DEVICE = 1
};

/**
 * @ingroup bus_configuration
 * @brief An enumeration of the different roles an I3C device can take.
 */
enum usbi3c_device_role {
	USBI3C_PRIMARY_CONTROLLER_ROLE = 0x1,		     ///< device has the primary I3C controller role
	USBI3C_TARGET_DEVICE_ROLE = 0x2,		     ///< device has I3C target device role
	USBI3C_TARGET_DEVICE_SECONDARY_CONTROLLER_ROLE = 0x3 ///< device has target device role capable of secondary controller
};

/**
 * @ingroup bus_configuration
 * @brief Enumeration of the different types of dynamic address assignments from a static address.
 */
enum usbi3c_asa_support {
	USBI3C_DEVICE_HAS_NO_STATIC_ADDRESS = 0x0,	 ///< I3C Target does not have a static address
	USBI3C_DEVICE_SUPPORTS_SETDASA = 0x1,		 ///< I3C Target supports SETDASA directed CCC
	USBI3C_DEVICE_SUPPORTS_SETAASA = 0x2,		 ///< I3C Target supports SETAASA broadcast CCC
	USBI3C_DEVICE_SUPPORTS_SETDASA_AND_SETAASA = 0x3 ///< I3C Target supports both SETDASA and SETAASA CCCs
};

/**
 * @ingroup bus_configuration
 * @brief A structure representing an I3C or an I2C device.
 *
 * When the I3C bus is being initialized, if the I3C controller is not aware of the target devices
 * on the I3C bus, users have to provide the target device table at the time of initialization, with
 * at least the I2C devices on the bus along with their static address. All I3C devices that support
 * dynamic address assignment could be left out of the table. This structure is used to represent an
 * I3C or I2C device that is to be added to the target device table using usbi3c_add_device_to_table().
 *
 * This structure is also used to represent devices in the target device table when users request them
 * by using the usbi3c_get_devices_in_table().
 */
struct usbi3c_target_device {
	enum usbi3c_target_device_type type; ///< Identifies if the target device is either an I3C or an I2C device
	uint8_t static_address;		     ///< The static address of the device (mandatory for I2C devices, optional for I3C devices)
	/* I3C specific configs */
	uint8_t dynamic_address;				///< The dynamic address assigned to the device by the I3C controller (not to be assigned by users)
	uint64_t provisioned_id;				///< A 48-bit ID all I3C devices must have (unless they have a static address)
	enum usbi3c_asa_support assignment_from_static_address; ///< Indicates the type of address assignment from static address the I3C device supports
	uint8_t dynamic_address_assignment_enabled;		///< Set to TRUE/FALSE to indicates whether the I3C device supports dynamic address assignment with ENTDAA
	uint8_t target_interrupt_request_enabled;		///< Set to TRUE/FALSE to indicate whether the controller should accept/reject interrupts from this device
	uint8_t controller_role_request_enabled;		///< Set to TRUE/FALSE to indicate whether the controller should accept/reject controller role request from this device
	uint8_t ibi_timestamp_enabled;				///< Set to TRUE/FALSE to indicate whether the controller should time-stamp IBIs from this device
	uint32_t max_ibi_payload_size;				///< indicates the maximum IBI payload size that this I3C device is allowed to send for an IBI
};

struct usbi3c_context;

struct usbi3c_device;

#ifdef __cplusplus
extern "C" {
#endif

/* usb setup */
struct usbi3c_context *usbi3c_init(void);
int usbi3c_get_devices(struct usbi3c_context *ctx, const uint16_t vendor_id, const uint16_t product_id, struct usbi3c_device ***devices);
int usbi3c_set_device(struct usbi3c_context *ctx, const uint16_t vendor_id, const uint16_t product_id);
void usbi3c_deinit(struct usbi3c_context **usbi3c);
void usbi3c_device_deinit(struct usbi3c_device **usbi3c_dev);
struct usbi3c_device *usbi3c_ref_device(struct usbi3c_device *usbi3c_dev);
void usbi3c_unref_device(struct usbi3c_device *usbi3c_dev);
void usbi3c_free_devices(struct usbi3c_device ***devices);
int usbi3c_get_usb_error(struct usbi3c_device *usbi3c_dev);
unsigned int usbi3c_set_timeout(struct usbi3c_device *usbi3c_dev, unsigned int timeout);

/* bus functions */
int usbi3c_initialize_device(struct usbi3c_device *usbi3c_dev);
int device_request_hotjoin(struct usbi3c_device *usbi3c_dev);
int usbi3c_request_i3c_controller_role(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_i3c_controller_role_handoff(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_regular_ibi(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_hot_join(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_regular_ibi_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_hot_join_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_enable_i3c_controller_role_request_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_i3c_bus(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_i3c_controller_role_handoff(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_regular_ibi(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_hot_join(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_regular_ibi_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_hot_join_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_disable_i3c_controller_role_request_wake(struct usbi3c_device *usbi3c_dev);
int usbi3c_exit_hdr_mode_for_recovery(struct usbi3c_device *usbi3c_dev);

/* target device functions */
int usbi3c_get_address_list(struct usbi3c_device *usbi3c_dev, uint8_t **list);
int usbi3c_set_target_device_config(struct usbi3c_device *usbi3c_dev, uint8_t address, uint8_t config);
int usbi3c_set_target_device_max_ibi_payload(struct usbi3c_device *usbi3c_dev, uint8_t address, uint32_t max_payload);
int usbi3c_change_i3c_device_address(struct usbi3c_device *usbi3c_dev, uint8_t current_address, uint8_t new_address, on_address_change_fn on_address_change_cb, void *data);
int usbi3c_request_i3c_controller_role(struct usbi3c_device *usbi3c_dev);
int usbi3c_add_device_to_table(struct usbi3c_device *usbi3c_dev, struct usbi3c_target_device device);
int usbi3c_get_target_device_table(struct usbi3c_device *usbi3c_dev, struct usbi3c_target_device ***devices);
void usbi3c_free_target_device_table(struct usbi3c_target_device ***devices);

/* get target device info */
int usbi3c_get_device_address(struct usbi3c_device *usbi3c_dev);
int usbi3c_get_target_BCR(struct usbi3c_device *usbi3c_dev, uint8_t address);
int usbi3c_get_target_DCR(struct usbi3c_device *usbi3c_dev, uint8_t address);
int usbi3c_get_target_type(struct usbi3c_device *usbi3c_dev, uint8_t address);
enum usbi3c_device_role usbi3c_get_device_role(struct usbi3c_device *usbi3c_dev);
int usbi3c_get_i3c_mode(struct usbi3c_device *usbi3c_dev, uint8_t *transfer_mode, uint8_t *transfer_rate, uint8_t *tm_specific_info);
int usbi3c_get_request_reattempt_max(struct usbi3c_device *usbi3c_dev, unsigned int *reattempt_max);
int usbi3c_get_target_device_config(struct usbi3c_device *usbi3c_dev, uint8_t address, uint8_t *config);
int usbi3c_get_target_device_max_ibi_payload(struct usbi3c_device *usbi3c_dev, uint8_t address, uint32_t *max_payload);
int usbi3c_get_timeout(struct usbi3c_device *usbi3c_dev, unsigned int *timeout);
int usbi3c_device_is_active_controller(struct usbi3c_device *usbi3c_dev);

/* Event functions */
void usbi3c_on_bus_error(struct usbi3c_device *usbi3c_dev, on_bus_error_fn on_bus_error_cb, void *data);
void usbi3c_on_hotjoin(struct usbi3c_device *usbi3c_dev, on_hotjoin_fn on_hotjoin, void *data);
void usbi3c_on_ibi(struct usbi3c_device *usbi3c_dev, on_ibi_fn on_ibi_cb, void *data);
int usbi3c_on_controller_event(struct usbi3c_device *usbi3c_dev, on_controller_event_fn on_controller_event_cb, void *data);
int usbi3c_on_vendor_specific_response(struct usbi3c_device *usbi3c_dev, on_vendor_response_fn on_vendor_response_cb, void *data);

/* bulk transfer functions */
void usbi3c_set_i3c_mode(struct usbi3c_device *usbi3c_dev, uint8_t transfer_mode, uint8_t transfer_rate, uint8_t tm_specific_info);
void usbi3c_set_request_reattempt_max(struct usbi3c_device *usbi3c_dev, unsigned int reattempt_max);
void usbi3c_free_responses(struct list **responses);
int usbi3c_enqueue_command(struct usbi3c_device *usbi3c_dev,
			   uint8_t target_address,
			   enum usbi3c_command_direction command_direction,
			   enum usbi3c_command_error_handling error_handling,
			   uint32_t data_size,
			   unsigned char *data,
			   on_response_fn on_response_cb,
			   void *user_data);
int usbi3c_enqueue_ccc(struct usbi3c_device *usbi3c_dev,
		       uint8_t target_address,
		       enum usbi3c_command_direction command_direction,
		       enum usbi3c_command_error_handling error_handling,
		       uint8_t ccc,
		       uint32_t data_size,
		       unsigned char *data,
		       on_response_fn on_response_cb,
		       void *user_data);
int usbi3c_enqueue_ccc_with_defining_byte(struct usbi3c_device *usbi3c_dev,
					  uint8_t target_address,
					  enum usbi3c_command_direction command_direction,
					  enum usbi3c_command_error_handling error_handling, uint8_t ccc,
					  uint8_t defining_byte,
					  uint32_t data_size,
					  unsigned char *data,
					  on_response_fn on_response_cb,
					  void *user_data);
int usbi3c_enqueue_target_reset_pattern(struct usbi3c_device *usbi3c_dev, on_response_fn on_response_cb, void *user_data);
struct list *usbi3c_send_commands(struct usbi3c_device *usbi3c_dev, uint8_t dependent_on_previous, int timeout);
int usbi3c_submit_vendor_specific_request(struct usbi3c_device *usbi3c_dev, unsigned char *data, uint32_t data_size);
int usbi3c_submit_commands(struct usbi3c_device *usbi3c_dev, uint8_t dependent_on_previous);
int usbi3c_request_i3c_controller_role(struct usbi3c_device *usbi3c_dev);

#ifdef __cplusplus
}
#endif

#endif /* __libusbi3c_h__ */
