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

#ifndef __usbi3c_spec_i__
#define __usbi3c_spec_i__

#include <stdint.h>

#include "usbi3c_commands.h"

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

/* size in bytes of a DWORD used by data structures in the spec */
#define DWORD_SIZE sizeof(DWORD)

#define NO_OFFSET 0

/* Default I3C transfer mode and rate */
#define DEFAULT_TRANSFER_MODE USBI3C_I3C_SDR_MODE
#define DEFAULT_TRANSFER_RATE USBI3C_I3C_RATE_2_MHZ

/****************
 * Capability   *
 ****************/

/* Capability buffer data size */
#define CAPABILITY_HEADER_SIZE sizeof(struct capability_header)
#define CAPABILITY_BUS_SIZE sizeof(struct capability_bus)
#define CAPABILITY_DEVICE_SIZE sizeof(struct capability_device_entry)

/* Capability buffer offsets */
#define CAPABILITY_HEADER_OFFSET NO_OFFSET
#define CAPABILITY_BUS_OFFSET (CAPABILITY_HEADER_OFFSET + CAPABILITY_HEADER_SIZE)
#define CAPABILITY_DEVICES_OFFSET(buffer) \
	(CAPABILITY_BUS_OFFSET + CAPABILITY_BUS_SIZE + GET_CAPABILITY_BUS(buffer)->transfer_mode_extended_cap_len)

/* Capability buffer accessors */
#define GET_CAPABILITY_HEADER(buffer) \
	((struct capability_header *)buffer)
#define GET_CAPABILITY_BUS(buffer) \
	((struct capability_bus *)(buffer + CAPABILITY_BUS_OFFSET))
#define GET_CAPABILITY_DEVICE_N(buffer, i) \
	((struct capability_device_entry *)(buffer + (CAPABILITY_DEVICES_OFFSET(buffer) + (CAPABILITY_DEVICE_SIZE * i))))

/**
 * @struct capability_header
 * @brief I3C capability header.
 *
 * Data structure that represents the header of a response buffer from a
 * GET_I3C_CAPABILITY request.
 */
struct capability_header {
	DWORD total_length : 16; ///< Total length in bytes of GET_I3C_CAPABILITY request
	DWORD device_role : 2;	 ///< indicates the role of the I3C device
	DWORD data_type : 2;	 ///< indicates the type of data in the I3C Capability data structure
	DWORD reserved : 4;	 ///< shall be set to all zeros
	DWORD error_code : 8;	 ///< indicates if the I3C device contains the I3C Capability data structure
} __attribute__((packed));

/**
 * @struct capability_bus
 * @brief Capabilities for the I3C device connected via USB.
 */
struct capability_bus {
	DWORD i3c_device_address : 8;		   ///< Device Address (optional)
	DWORD devices_present : 8;		   ///< indicates the type of I2C Target devices present on the I3C Bus
	DWORD reserved : 4;			   ///< shall be set to all zeros
	DWORD handoff_controller_role : 1;	   ///< indicates if device is capable of handing off the I3C Controller role
	DWORD hot_join_capability : 1;		   ///< indicates if I3C Device is capable of handling Hot-Joins
	DWORD in_band_interrupt_capability : 1;	   ///< indicates if I3C Device is capable of handling IBIs
	DWORD reserved_2 : 1;			   ///< shall be set to all zeros
	DWORD pending_read_capability : 1;	   ///< indicates if this I3C Device supports pending read for an IBI
	DWORD self_initiated : 1;		   ///< indicates if pending read is initiated by the device or the host
	DWORD delayed_pending_read : 1;		   ///< indicates if I3C Device performs immediate or delayed pending read
	DWORD pending_read_sdr : 1;		   ///< indicates if this I3C Device supports pending read in SDR mode
	DWORD pending_read_hdr : 1;		   ///< indicates if this I3C Device supports pending read in HDR mode
	DWORD reserved_3 : 2;			   ///< shall be set to all zeros
	DWORD single_command_pending_read : 1;	   ///< indicates if this I3C Device supports pending with a single or multiple commands
	DWORD mipi_minor_version : 16;		   ///< MIPI I3C Minor Version Number
	DWORD mipi_major_version : 16;		   ///< MIPI I3C Major Version Number
	DWORD mipi_disco_minor_version : 16;	   ///< MIPI I3C DisCo Minor Version Number
	DWORD mipi_disco_major_version : 16;	   ///< MIPI I3C DisCo Major Version Number
	DWORD i2c_data_transfer_rates : 8;	   ///< I2C data transfer rates
	DWORD reserved_4 : 8;			   ///< shall be set to all zeros
	DWORD clock_frequency_i2c_udr1 : 16;	   ///< Clock frequency in Hz for I2C UDR1 transfer mode
	DWORD clock_frequency_i2c_udr2 : 16;	   ///< Clock frequency in Hz for I2C UDR2 transfer mode
	DWORD clock_frequency_i2c_udr3 : 16;	   ///< Clock frequency in Hz for I2C UDR3 transfer mode
	DWORD i3c_data_transfer_modes : 8;	   ///< indicates all the I3C data transfer modes supported by the I3C device
	DWORD i3c_data_transfer_rates : 8;	   ///< indicates all the I3C data transfer rates supported by the I3C device
	DWORD transfer_mode_extended_cap_len : 16; ///< Length of Transfer Mode Extended Capabilities in Bytes
	DWORD clock_frequency_i3c_udr1;		   ///< contains the clock frequency in KHz when using I3C UDR1 transfer rate
	DWORD clock_frequency_i3c_udr2;		   ///< contains the clock frequency in KHz when using I3C UDR2 transfer rate
	DWORD max_ibi_payload_size;		   ///< Max IBI payload size
} __attribute__((packed));

/**
 * @struct capability_device_entry
 * @brief Capability of target devices
 */
struct capability_device_entry {
	DWORD address : 8;		     ///< Static address
	DWORD ibi_prioritization : 8;	     ///< Target device IBI priority, lower value higher priority
	DWORD pid_lo : 16;		     ///< Provisional ID low
	DWORD pid_hi;			     ///< Provisional ID high
	DWORD mipi_disco_minor_version : 16; ///< MIPI I3C DisCo Minor Version Number
	DWORD mipi_disco_major_version : 16; ///< MIPI I3C DisCo Major Version Number
	DWORD max_ibi_pending_size;	     ///< Maximum amount of data to send as pending read for IBI to the I3C controller
} __attribute__((packed));

/**
 * @brief Enumerates the type of data in the I3C capability which describe the awareness the
 * I3C device has about the target devices.
 */
enum capability_data_type {
	STATIC_DATA = 0x1,    ///< I3C device is aware of the target devices on the I3C bus
	NO_STATIC_DATA = 0x2, ///< I3C device is not aware of the target devices on the I3C bus
	DYNAMIC_DATA = 0x3    ///< I3C device aware of the target devices through info sent from the Host
};

/**
 * @brief Enumerates the states of presence of the capability data in the I3C device.
 */
enum capability_error_code {
	DEVICE_CONTAINS_CAPABILITY_DATA = 0x00,	       ///< I3C device contains the data
	DEVICE_DOES_NOT_CONTAIN_CAPABILITY_DATA = 0xFF ///< I3C device does not contain the data
};

/**
 * @enum capability_bus_devices_present
 *
 * @brief Indicates the kind of I2C devices present in the bus
 */
enum capability_bus_devices_present {
	NO_I2C_TARGET_DEVICES = 0x0,
	I2C_TARGET_DEVICES_50NS = 0x1,
	I2C_TARGET_DEVICES_NOT_50NS = 0x2,
	I2C_TARGET_MIXED = 0x3
};

/**
 * @brief Enumeration of selectable I3C features.
 */
enum i3c_feature_selector {
	I3C_BUS = 0x01,				///< the I3C bus (used with CLEAR_FEATURE only)
	I3C_CONTROLLER_ROLE_HANDOFF = 0x02,	///< the I3C controller role handoff
	REGULAR_IBI = 0x03,			///< all regular in-band Interrupts from I3C target devices
	HOT_JOIN = 0x04,			///< the Hot-Join feature
	RESERVED_SELECTOR = 0x05,		///< Reserved
	REGULAR_IBI_WAKE = 0x6,			///< the USB remote wake from regular in-band interrupts
	HOT_JOIN_WAKE = 0x07,			///< the USB remote wake from regular Hot-Join
	I3C_CONTROLLER_ROLE_REQUEST_WAKE = 0x8, ///< the USB remote wake from I3C controller role request
	HDR_MODE_EXIT_RECOVERY = 0x9		///< forces I3C target devices to exit HDR mode (used with CLEAR_FEATURE only)
};

/*****************
 * Target Device *
 *****************/

/* Target device table size */
#define TARGET_DEVICE_HEADER_SIZE sizeof(struct target_device_table_header)
#define TARGET_DEVICE_ENTRY_SIZE sizeof(struct target_device_table_entry)

/* Target device table offsets */
#define TARGET_DEVICE_HEADER_OFFSET NO_OFFSET
#define TARGET_DEVICE_ENTRY_OFFSET (TARGET_DEVICE_HEADER_OFFSET + TARGET_DEVICE_HEADER_SIZE)

/* Target device table accessors */
#define GET_TARGET_DEVICE_TABLE_HEADER(buffer) \
	((struct target_device_table_header *)buffer)
#define GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, i) \
	((struct target_device_table_entry *)(buffer + (TARGET_DEVICE_ENTRY_OFFSET + (TARGET_DEVICE_ENTRY_SIZE * i))))

/**
 * @struct target_device_table_header
 * @brief Header for GET_TARGET_DEVICE_TABLE request buffer.
 */
struct target_device_table_header {
	DWORD table_size : 16; ///< the total size (in bytes) of the Target Device Table
	DWORD reserved : 16;   ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @struct target_device_table_entry
 * @brief Entry represent target device connected to I3C Bus.
 */
struct target_device_table_entry {
	DWORD address : 8;		    ///< target device address
	DWORD target_interrupt_request : 1; ///< indicates if the I3C controller accepts interrupts from this target device
	DWORD controller_role_request : 1;  ///< indicates if the I3C controller accepts controller role requests from this target device
	DWORD ibi_timestamp : 1;	    ///< enables or disables time-stamping of IBIs from the target device
	DWORD asa : 2;			    ///< configures wether this device supports assignment from static address
	DWORD daa : 1;			    ///< configures wether this device should use dynamic address assignment with ENTDAA
	DWORD reserved_1 : 2;		    ///< shall be set to all zeros
	DWORD change_flags : 4;		    ///< indicate if there is a change in R/W fields
	DWORD target_type : 4;		    ///< indicates if the target device is an I2C or I3C device
	DWORD pending_read_capability : 1;  ///< indicates if the target device supports IBI pending read capability
	DWORD valid_pid : 1;		    ///< indicates if the Target device has a valid 48-bit PID (in PID_HI + PID_LO)
	DWORD reserved_2 : 6;		    ///< shall be set to all zeros
	DWORD max_ibi_payload_size;	    ///< Max IBI payload to send to I3C Controller
	DWORD bcr : 8;			    ///< Bus Characteristic Register
	DWORD dcr : 8;			    ///< Device Characteristic Register
	DWORD pid_lo : 16;		    ///< Provisional ID low
	DWORD pid_hi;			    ///< Provisional ID high
} __attribute__((packed));

/**
 * @brief Enumeration of address assignment configuration types.
 */
enum target_device_table_assignment_from_static_address {
	TARGET_DEVICE_DOES_NOT_HAVE_STATIC_ADDRESS = 0,
	TARGET_DEVICE_SUPPORTS_SETDASA = 1,
	TARGET_DEVICE_SUPPORTS_SETAASA = 2,
	TARGET_DEVICE_SUPPORTS_SETDASA_AND_SETAASA = 3
};

/**
 * @brief Enumeration of address assignment with ENTDAA configuration types.
 */
enum target_device_table_dynamic_address_assignment_with_entdaa {
	TARGET_DEVICE_SHOULD_NOT_USE_ENTDAA = 0,
	TARGET_DEVICE_SHOULD_USE_ENTDAA = 1
};

/**
 * @brief Enumeration of changes in R/W fields.
 */
enum target_device_table_change_flags {
	NO_CHANGE = 0,
	CHANGE_IN_TARGET_ADDRESS_FIELD = 1,
	CHANGE_IN_TARGET_INTERRUPT_REQUEST_FIELD = 2,
	CHANGE_IN_CONTROLLER_ROLE_REQUEST_FIELD = 3,
	CHANGE_IN_IBI_TIMESTAMP_FIELD = 4
};

/*****************
 * Device Config *
 *****************/

/* Target device configuration size */
#define TARGET_DEVICE_CONFIG_HEADER_SIZE sizeof(struct target_device_config_header)
#define TARGET_DEVICE_CONFIG_ENTRY_SIZE sizeof(struct target_device_config_entry)

/* Target device configuration offsets */
#define TARGET_DEVICE_CONFIG_HEADER_OFFSET NO_OFFSET
#define TARGET_DEVICE_CONFIG_ENTRY_OFFSET (TARGET_DEVICE_CONFIG_HEADER_OFFSET + TARGET_DEVICE_CONFIG_HEADER_SIZE)

/* Target device configuration accessors */
#define GET_TARGET_DEVICE_CONFIG_HEADER(buffer) \
	((struct target_device_config_header *)buffer)
#define GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, i) \
	((struct target_device_config_entry *)(buffer + (TARGET_DEVICE_CONFIG_ENTRY_OFFSET + (TARGET_DEVICE_CONFIG_ENTRY_SIZE * i))))

/**
 * @struct target_device_config_header
 * @brief Header for SET_TARGET_DEVICE_CONFIG request buffer.
 */
struct target_device_config_header {
	DWORD config_change_command_type : 4; ///< contains the config change command type
	DWORD reserved_1 : 4;		      ///< shall be set to all zeros
	DWORD numentries : 8;		      ///< Number of entries
	DWORD reserved_2 : 16;		      ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @struct target_device_config_entry
 * @brief Entry to change info in target device
 */
struct target_device_config_entry {
	DWORD address : 8;		    ///< target address
	DWORD target_interrupt_request : 1; ///< configures if controller accepts interrupts from this target device
	DWORD controller_role_request : 1;  ///< configures if controller accepts controller role requests from this target device
	DWORD ibi_timestamp : 1;	    ///< enables or disables time-stamping of IBIs from this target device
	DWORD reserved : 21;		    ///< shall be set to all zeros
	DWORD max_ibi_payload_size;	    ///< Max payload IBI size (Max. 4GB)
} __attribute__((packed));

/**
 * @enum config_change_command_type
 *
 * @brief Type of change command.
 */
enum config_change_command_type {
	CHANGE_CONFIG_COMMAND_TYPE = 0x1,
	CLEAR_CONFIG_COMMAND_TYPE = 0x2
};

/******************
 * Address Change *
 ******************/

/* Address change data size */
#define TARGET_DEVICE_ADDRESS_CHANGE_HEADER_SIZE sizeof(struct target_device_address_change_header)
#define TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_SIZE sizeof(struct target_device_address_change_entry)

/* Address change buffer offsets */
#define TARGET_DEVICE_ADDRESS_CHANGE_HEADER_OFFSET NO_OFFSET
#define TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_OFFSET \
	(TARGET_DEVICE_ADDRESS_CHANGE_HEADER_OFFSET + TARGET_DEVICE_ADDRESS_CHANGE_HEADER_SIZE)

/* Address change buffer accessors */
#define GET_TARGET_DEVICE_ADDRESS_CHANGE_HEADER(buffer) \
	((struct target_device_address_change_header *)buffer)
#define GET_TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_N(buffer, i) \
	((struct target_device_address_change_entry *)(buffer + (TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_OFFSET + (TARGET_DEVICE_ADDRESS_CHANGE_ENTRY_SIZE * i))))

/**
 * @brief Data structure of an address change header.
 */
struct target_device_address_change_header {
	DWORD address_change_command_type : 4; ///< the command type
	DWORD reserved1 : 4;		       ///< shall be set to all zeros
	DWORD numentries : 8;		       ///< number of devices for which the Host intends to change the address
	DWORD reserved2 : 16;		       ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Data structure of an address change entry.
 *
 * The data structure of an address change contains a header + one or more entries.
 */
struct target_device_address_change_entry {
	DWORD current_address : 8; ///< the current dynamic address
	DWORD new_address : 8;	   ///< the new dynamic address to be assigned
	DWORD pid_lo : 16;	   ///< bits 15:0 of the provisional ID
	DWORD pid_hi;		   ///< bits 47:16 of the provisional ID
} __attribute__((packed));

/**
 * @brief Enumeration of address command types.
 */
enum address_change_command_type {
	ADDRESS_CHANGE_COMMAND_TYPE = 0x1
};

/*************************
 * Address Change Result *
 *************************/

/* Address change result buffer size */
#define TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER_SIZE sizeof(struct target_device_address_change_result_header)
#define TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_SIZE sizeof(struct target_device_address_change_result_entry)

/* Address change result buffer offsets */
#define TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER_OFFSET NO_OFFSET
#define TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_OFFSET \
	(TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER_OFFSET + TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER_SIZE)

/* Address change result buffer accessors */
#define GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_HEADER(buffer) \
	((struct target_device_address_change_result_header *)buffer)
#define GET_TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_N(buffer, i) \
	((struct target_device_address_change_result_entry *)(buffer + (TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_OFFSET + (TARGET_DEVICE_ADDRESS_CHANGE_RESULT_ENTRY_SIZE * i))))

/**
 * @brief Data structure of an address change result header.
 */
struct target_device_address_change_result_header {
	DWORD size : 8;	      ///< total size (in bytes) of the address change result data structure (header + entries)
	DWORD numentries : 8; ///< number of devices for which the Host attempted to change the address
	DWORD reserved : 16;  ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Data structure of an address change result entry.
 *
 * The data structure of an address change result contains a header + one or more entries.
 */
struct target_device_address_change_result_entry {
	DWORD current_address : 8; ///< the current dynamic address
	DWORD new_address : 8;	   ///< the new dynamic address assigned
	DWORD status : 1;	   ///< indicates if this specific address was successfully changed (0b = success, 1b = failure)
	DWORD reserved : 15;	   ///< shall be set to all zeros
} __attribute__((packed));

/**********************
 * NOTIFICATION TYPES *
 **********************/

/* notification data size */
#define NOTIFICATION_SIZE (sizeof(struct notification_format))

/* size of notification handler table */
#define NOTIFICATION_HANDLERS_SIZE 7

#define GET_NOTIFICATION_FORMAT(buffer) \
	((struct notification_format *)buffer)

/**
 * @struct notification_format
 * @brief Represents the notifications that an I3C Function may send to the Host.
 */
struct notification_format {
	DWORD type : 8;	    ///< Type of notification
	DWORD code : 16;    ///< Notification Value (it depends on the type of notification)
	DWORD reserved : 8; ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @enum notification_type
 *
 * Enumerates the type of notifications that an I3C Function may send to the Host.
 */
enum notification_type {
	NOTIFICATION_I3C_BUS_INITIALIZATION_STATUS = 0x1, ///< Result of initialize I3C bus request.
	NOTIFICATION_ADDRESS_CHANGE_STATUS = 0x2,	  ///< Result of an address change request or a successful hot-join.
	NOTIFICATION_I3C_BUS_ERROR = 0x3,		  ///< Result of an error in the I3C Bus.
	NOTIFICATION_I3C_IBI = 0x4,			  ///< IBI or Hot-join to the controller
	NOTIFICATION_ACTIVE_I3C_CONTROLLER_EVENT = 0x5,	  ///< Target device notification from I3C Controller
	NOTIFICATION_STALL_ON_NACK = 0x6,		  ///< Sent to indicate the I3C Controller stalled the execution of commands.
};

/**
 * @enum notification_code_i3c_bus_init
 *
 * Codes for I3C_BUS_INITIALIZATION notification type
 */
enum notification_code_i3c_bus_init {
	I3C_BUS_UNINITIALIZED = -1,				     ///< Uninitialized I3C Bus (this status is not part of the spec, this is for lib purposes)
	SUCCESSFUL_I3C_BUS_INITIALIZATION = 0x0,		     ///< Successful I3C Bus initialization.
	FAILURE_ENABLE_I3C_BUS = 0x1,				     ///< Failure to enable I3C Bus.
	FAILURE_DEVICE_DISCOVERY_N_DYNAMIC_ADDRESS_ASSIGNMENT = 0x2, ///< Failure with device discovery and dynamic address assignment.
	FAILURE_TARGET_DEVICE_TABLE_GENERATION_UPDATE = 0x3,	     ///< Failure with device target table generation/update.
};

/**
 * @enum notification_code_address_change
 *
 * Codes for ADDRESS_CHANGE_STATUS notification type
 */
enum notification_code_address_change {
	ALL_ADDRESS_CHANGE_SUCCEEDED = 0x0,	    ///< All addresses changed successfully
	SOME_ADDRESS_CHANGE_FAILED = 0x1,	    ///< One or more address change requests failed
	HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED = 0x2, ///< Hotjoin address assigned successfully
	HOTJOIN_ADDRESS_ASSIGNMENT_FAILED = 0x3,    ///< Hotjoin Failure with address assigned
};

/**
 * @enum notification_code_i3c_bus_error
 *
 * Codes for I3C_BUS_ERROR notification type
 */
enum notification_code_i3c_bus_error {
	CCC_FORMAT_INCORRECT = 0x1,			 ///< CCC format incorrect
	DATA_TRANSMITTED_INCORRECT = 0x2,		 ///< Data transmitted incorrect
	BROADCAST_ADDRESS_NACKD = 0x3,			 ///< Broadcast Address NACK'd
	NEW_I3C_CONTROLLER_FAILS_TO_DRIVE_I3C_BUS = 0x4, ///< New I3C controller fails to drive the I3C Bus.
};

/**
 * @enum notification_code_i3c_ibi
 *
 * Codes for I3C_IBI notification type
 */
enum notification_code_i3c_ibi {
	REGULAR_IBI_NO_PAYLOAD_ACK_BY_I3C_CONTROLLER = 0x1,		 ///< Regular IBI without payload ACK’d by the I3C Controller.
	REGULAR_IBI_PAYLOAD_ACK_BY_I3C_CONTROLLER = 0x2,		 ///< Regular IBI with payload ACK’d by the I3C Controller.
	IBI_AUTOCOMMAND_INITIATED_BY_I3C_CONTROLLER = 0x3,		 ///< IBI with Auto-Command/Private Read initiated by the I3C Controller
	REGULAR_IBI_NACKD_BY_I3C_CONTROLLER = 0x4,			 ///< Regular IBI NACK’d by the I3C Controller.
	HOTJOIN_IBI_ACKD_BY_I3C_CONTROLLER = 0x5,			 ///< Hot-Join IBI ACK’d by the I3C Controller
	HOTJOIN_IBI_NACKD_BY_I3C_CONTROLLER = 0x6,			 ///< Hot-Join IBI NACK’d by the I3C Controller
	SECONDARY_CONTROLLER_ROLE_REQUEST_ACKD_BY_I3C_CONTROLLER = 0x7,	 ///< Secondary Controller role request ACK’d by the I3C Controller
	SECONDARY_CONTROLLER_ROLE_REQUEST_NACKD_BY_I3C_CONTROLLER = 0x8, ///< Secondary Controller role request NACK’d by the I3C Controller
};

/**
 * @enum notification_code_active_i3c_controller
 *
 * Codes for ACTIVE_I3C_CONTROLLER_EVENT notification type
 */
enum notification_code_active_i3c_controller {
	RECEIVED_CCC = 0x2,	      ///< Received CCC
	RECEIVED_READ_REQUEST = 0x3,  ///< Received Read Request
	RECEIVED_WRITE_REQUEST = 0x4, ///< Received Write Request
};

/*****************
 * Bulk transfer *
 *****************/

/* Bulk transfer data size */
#define BULK_TRANSFER_HEADER_SIZE (sizeof(struct bulk_transfer_header))

/* Bulk transfer data offsets */
#define BULK_TRANSFER_HEADER_OFFSET NO_OFFSET

/* Bulk transfer accessors */
#define GET_BULK_TRANSFER_HEADER(buffer) \
	((struct bulk_transfer_header *)buffer)

/**
 * @brief Data structure of a bulk transfer header.
 *
 * Bulk transfers can either be bulk requests, or bulk responses.
 * This data structure is common for both types of bulk transfers
 * (dependent_on_previous only applies to bulk request transfer).
 */
struct bulk_transfer_header {
	DWORD tag : 2;			 ///< indicates the type of bulk transfer (regular, interrupt, or vendor specific)
	DWORD dependent_on_previous : 1; ///< indicates if the bulk request is dependent on the previous request (set to zero in bulk responses)
	DWORD reserved : 29;		 ///< shall be set to all zeros
} __attribute__((packed));

/****************
 * Bulk request *
 ****************/

/* Bulk request data size */
#define BULK_REQUEST_COMMAND_BLOCK_HEADER_SIZE (sizeof(struct bulk_request_command_block_header))
#define BULK_REQUEST_COMMAND_DESCRIPTOR_SIZE (sizeof(struct bulk_request_command_descriptor))

/* Bulk request data offsets */
#define BULK_REQUEST_COMMAND_BLOCK_HEADER_OFFSET NO_OFFSET
#define BULK_REQUEST_COMMAND_DESCRIPTOR_OFFSET (BULK_REQUEST_COMMAND_BLOCK_HEADER_OFFSET + BULK_REQUEST_COMMAND_BLOCK_HEADER_SIZE)
#define BULK_REQUEST_DATA_BLOCK_OFFSET (BULK_REQUEST_COMMAND_DESCRIPTOR_OFFSET + BULK_REQUEST_COMMAND_DESCRIPTOR_SIZE)

/* Bulk request data accessors */
#define GET_BULK_REQUEST_COMMAND_BLOCK_HEADER(buffer) \
	((struct bulk_request_command_block_header *)(buffer + BULK_REQUEST_COMMAND_BLOCK_HEADER_OFFSET))
#define GET_BULK_REQUEST_COMMAND_DESCRIPTOR(buffer) \
	((struct bulk_request_command_descriptor *)(buffer + BULK_REQUEST_COMMAND_DESCRIPTOR_OFFSET))
#define GET_BULK_REQUEST_DATA_BLOCK(buffer, padding) \
	((unsigned char *)(buffer + BULK_REQUEST_DATA_BLOCK_OFFSET + padding))

/**
 * @brief Data structure of a bulk request command block header.
 */
struct bulk_request_command_block_header {
	DWORD request_id : 16; ///< monotonically increasing request number
	DWORD has_data : 1;    ///< indicates if there is a data block appended
	DWORD reserved : 15;   ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Data structure of a bulk request command descriptor.
 */
struct bulk_request_command_descriptor {
	DWORD command_type : 3;	    ///< contains the command type
	DWORD read_or_write : 1;    ///< indicates the direction of the command
	DWORD error_handling : 4;   ///< condition for the I3C controller to abort subsequent commands
	DWORD target_address : 8;   ///< the target device address
	DWORD transfer_mode : 5;    ///< indicates the transfer mode of the I3C or I2C command
	DWORD transfer_rate : 3;    ///< indicates the transfer rate for the selected transfer mode
	DWORD tm_specific_info : 8; ///< reserved for transfer mode specific information
	DWORD defining_byte : 8;    ///< contains the defining byte for the CCC (if applicable)
	DWORD ccc : 8;		    ///< contains the value for CCC (if applicable)
	DWORD reserved_1 : 16;	    ///< shall be set to all zeros
	DWORD data_length : 22;	    ///< indicates the number of bytes to be transferred
	DWORD reserved_2 : 10;	    ///< shall be set to all zeros
	DWORD reserved_3;	    ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Enumeration of bulk request transfer types.
 */
enum bulk_request_transfer_header_tag {
	REGULAR_BULK_REQUEST = 0x0,
	RESERVED_TAG = 0x1,
	VENDOR_SPECIFIC_BULK_REQUEST = 0x2
};

/*****************
 * Bulk response *
 *****************/

/* Bulk response data size */
#define BULK_RESPONSE_BLOCK_HEADER_SIZE (sizeof(struct bulk_response_block_header))
#define BULK_RESPONSE_DESCRIPTOR_SIZE (sizeof(struct bulk_response_descriptor))

/* Bulk response data offsets */
#define BULK_RESPONSE_BLOCK_HEADER_OFFSET NO_OFFSET
#define BULK_RESPONSE_DESCRIPTOR_OFFSET (BULK_RESPONSE_BLOCK_HEADER_OFFSET + BULK_RESPONSE_BLOCK_HEADER_SIZE)
#define BULK_RESPONSE_DATA_BLOCK_OFFSET (BULK_RESPONSE_DESCRIPTOR_OFFSET + BULK_RESPONSE_DESCRIPTOR_SIZE)

/* Bulk response data accessors */
#define GET_BULK_RESPONSE_BLOCK_HEADER(buffer) \
	((struct bulk_response_block_header *)(buffer + BULK_RESPONSE_BLOCK_HEADER_OFFSET))
#define GET_BULK_RESPONSE_DESCRIPTOR(buffer) \
	((struct bulk_response_descriptor *)(buffer + BULK_RESPONSE_DESCRIPTOR_OFFSET))
#define GET_BULK_RESPONSE_DATA_BLOCK(buffer, padding) \
	((unsigned char *)(buffer + BULK_RESPONSE_DATA_BLOCK_OFFSET + padding))

/**
 * @brief Data structure of a bulk response block header.
 */
struct bulk_response_block_header {
	DWORD request_id : 16; ///< contains the corresponding request ID for a command
	DWORD reserved_1 : 8;  ///< shall be set to all zeros
	DWORD has_data : 1;    ///< indicates if there is a data block appended
	DWORD attempted : 1;   ///< indicates if the command command was attempted
	DWORD reserved_2 : 6;  ///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Data structure of a bulk response descriptor.
 */
struct bulk_response_descriptor {
	DWORD data_length : 22; ///< indicates the number of bytes to be transferred
	DWORD reserved_1 : 6;	///< shall be set to all zeros
	DWORD error_status : 4; ///< indicates the status for the processed command (0h indicates success)
	DWORD reserved_2;	///< shall be set to all zeros
} __attribute__((packed));

/**
 * @brief Enumeration of bulk response transfer types.
 */
enum bulk_response_transfer_header_tag {
	REGULAR_BULK_RESPONSE = 0x0,
	INTERRUPT_BULK_RESPONSE = 0x1,
	VENDOR_SPECIFIC_BULK_RESPONSE = 0x2
};

/**
 * @brief Data structure of a bulk IBI response header
 *
 * This structure match with bulk_transfer_header but has fields
 * specific for IBI response
 */
struct bulk_ibi_response_header {
	DWORD tag : 2;		///< field to identify that we have an IBI response should be 0x1
	DWORD reserved_1 : 14;	///< shall be set to all zeros
	DWORD sequence_id : 16; ///< sequence id to identify when a response start or is a continuation
} __attribute__((packed));

/**
 * @brief Data structure that describe an IBI response footer
 */
struct bulk_ibi_response_footer {
	DWORD target_address : 7; ///< address of the device that issued the IBI
	DWORD R_W : 1;		  ///< if IBI is Read or Write, 0 write, 1 read
	DWORD ibi_status : 1;	  ///< if IBI is acknowledged 0 if not 1
	DWORD error : 1;	  ///< if the IBI was caused by a command execution error
	DWORD ibi_timestamp : 1;  ///< if the IBI is timestamped
	DWORD ibi_type : 1;	  ///< if is a regular IBI 0, 1 if it is scheduled or from a secondary controller
	DWORD pending_read : 1;	  ///< if there is pending read this should be 1, it means there are additional IBI responses with this data
	DWORD last_byte : 1;	  ///< if this is the last data block for this IBI response
	DWORD bytes_valid : 2;	  ///< this is the number of bytes in the last DWORD that are part of the IBI response data
	DWORD reserved_1 : 16;	  ///< shall be set to all zeros
} __attribute__((packed));

struct mandatory_data_byte {
	BYTE specific_interrupt_id : 5; ///< Specific interrupt id to identify what has happened
	BYTE interrupt_group_id : 3;	///< Interrupt group identifier that this IBI belongs to
} __attribute__((packed));

/********************************
 * Vendor specific bulk request *
 ********************************/

/* Vendor specific bulk request data size */
#define VENDOR_SPECIFIC_REQUEST_HEADER_SIZE BULK_TRANSFER_HEADER_SIZE

/* Vendor specific bulk request data offsets */
#define VENDOR_SPECIFIC_REQUEST_HEADER_OFFSET BULK_TRANSFER_HEADER_OFFSET
#define VENDOR_SPECIFIC_BLOCK_OFFSET VENDOR_SPECIFIC_REQUEST_HEADER_SIZE

/* Vendor specific bulk request data accessors */
#define GET_VENDOR_SPECIFIC_REQUEST_HEADER(buffer) GET_BULK_TRANSFER_HEADER(buffer)
#define GET_VENDOR_SPECIFIC_BLOCK(buffer, padding) \
	((unsigned char *)(buffer + VENDOR_SPECIFIC_BLOCK_OFFSET + padding))

#endif
