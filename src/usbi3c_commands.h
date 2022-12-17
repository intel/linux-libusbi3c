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

#ifndef __libusbi3c_commands_h__
#define __libusbi3c_commands_h__

/**
 * @ingroup command_execution
 * @brief Enumerates the Read/Write direction of a regular command or CCC.
 */
enum usbi3c_command_direction {
	USBI3C_WRITE = 0x0, ///< Indicates the command will write data into one or many I3C devices
	USBI3C_READ = 0x1   ///< Indicates the command will read data from one or many I3C devices
};

/**
 * @ingroup command_execution
 * @brief Enumeration of dependency types for commands in a transfer.
 *
 * If this request is dependent on the previous request, and a command in the previous
 * request stalls on a NACK and keeps stalling after retrying, it will cause this request
 * to get cancelled as well, along with all the commands it contains.
 */
enum usbi3c_command_request_dependent_on_previous {
	USBI3C_NOT_DEPENDENT_ON_PREVIOUS = 0x0, ///< Sets the request as not dependent on the previous request
	USBI3C_DEPENDENT_ON_PREVIOUS = 0x1,	///< Sets the request as dependent on the previous request
};

/**
 * @ingroup command_execution
 * @brief Enumeration of error handling during I3C command execution.
 */
enum usbi3c_command_error_handling {
	USBI3C_TERMINATE_ON_ANY_ERROR = 0x0,			    ///< Terminate on any error (where NACK is an error)
	USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK = 0x1,	    ///< Terminate on any error except NACK (where NACK is not an error)
	USBI3C_DO_NOT_TERMINATE_ON_ERROR_INCLUDING_NACK = 0x2,	    ///< Don’t Terminate on error including NACK
	USBI3C_TERMINATE_ON_SHORT_READ = 0x3,			    ///< Terminate on Short Read
	USBI3C_TERMINATE_ON_ERROR_BUT_STALL_EXECUTION_ON_NACK = 0X4 ///< Terminate on any error, but stall execution on NACK
};

/**
 * @ingroup command_execution
 * @brief Enumeration of transfer modes for the I3C or I2C commands.
 */
enum usbi3c_command_transfer_mode {
	USBI3C_I3C_SDR_MODE = 0x0,     ///< I3C SDR Mode
	USBI3C_I3C_HDR_DDR_MODE = 0x1, ///< I3C HDR-DDR Mode
	USBI3C_I3C_HDR_TS_MODE = 0x2,  ///< I3C HDR-TS Mode
	USBI3C_I3C_DDR_BT_MODE = 0x3,  ///< I3C HDR-BT Mode
	USBI3C_I2C_MODE = 0x8	       ///< I2C Mode
};

/**
 * @ingroup command_execution
 * @brief Enumeration of transfer rates for the I3C modes.
 */
enum usbi3c_i3c_transfer_rate {
	USBI3C_I3C_RATE_2_MHZ = 0x0,	      ///< 2 MHz
	USBI3C_I3C_RATE_4_MHZ = 0x1,	      ///< 4 MHz
	USBI3C_I3C_RATE_6_MHZ = 0x2,	      ///< 6 MHz
	USBI3C_I3C_RATE_8_MHZ = 0x3,	      ///< 8 MHz
	USBI3C_I3C_RATE_12_5_MHZ = 0x4,	      ///< 12.5 MHz
	USBI3C_I3C_RATE_USER_DEFINED_1 = 0x5, ///< User defined I3C data rate 1
	USBI3C_I3C_RATE_USER_DEFINED_2 = 0x6  ///< User defined I3C data rate 2
};

/**
 * @ingroup command_execution
 * @brief Enumeration of transfer rates for the I2C mode.
 */
enum usbi3c_i2c_transfer_rate {
	USBI3C_I2C_RATE_100_KHZ = 0x0,	      ///< 100 KHz
	USBI3C_I2C_RATE_400_KHZ = 0x1,	      ///< 400 KHz
	USBI3C_I2C_RATE_1_MHZ = 0x2,	      ///< i MHz
	USBI3C_I2C_RATE_USER_DEFINED_1 = 0x3, ///< User defined I2C data rate 1
	USBI3C_I2C_RATE_USER_DEFINED_2 = 0x4, ///< User defined I2C data rate 2
	USBI3C_I2C_RATE_USER_DEFINED_3 = 0x5  ///< User defined I2C data rate 3
};

/**
 * @ingroup command_execution
 * @brief Enumeration of data presence in command responses.
 */
enum usbi3c_response_data_availability {
	USBI3C_RESPONSE_HAS_NO_DATA = 0x0, ///< The response to a command contains no data
	USBI3C_RESPONSE_HAS_DATA = 0x1	   ///< The response to a command contains data
};

/**
 * @ingroup command_execution
 * @brief Enumeration of command status.
 */
enum usbi3c_command_status {
	USBI3C_COMMAND_NOT_ATTEMPTED = 0x0, ///< Indicates that the command was not attempted
	USBI3C_COMMAND_ATTEMPTED = 0x1	    ///< Indicates that the command was attempted
};

/**
 * @ingroup command_execution
 * @brief Enumeration of command execution status.
 */
enum usbi3c_command_execution_status {
	USBI3C_SUCCEEDED = 0x0,				    ///< The command processed successfully
	USBI3C_FAILED_CRC_ERROR = 0x1,			    ///< The command failed with CRC error
	USBI3C_FAILED_PARITY_ERROR = 0x2,		    ///< The command failed with parity error
	USBI3C_FAILED_FRAME_ERROR = 0x3,		    ///< The command failed with a frame error
	USBI3C_FAILED_ADDRESS_ERROR = 0x4,		    ///< The command failed with an address header or broadcast address error
	USBI3C_FAILED_NACK = 0x5,			    ///< The command was not acknowledged by a target device
	USBI3C_FAILED_SHORT_READ_ERROR = 0x7,		    ///< The command failed with a short read error
	USBI3C_FAILED_I3C_CONTROLLER_ERROR = 0x8,	    ///< The command failed with an I3C controller error
	USBI3C_FAILED_TRANSFER_ERROR = 0x9,		    ///< The command failed to write data or there was an I3C bus transfer error
	USBI3C_FAILED_BAD_COMMAND = 0xA,		    ///< The command is bad or not supported
	USBI3C_FAILED_COMMAND_ABORTED_WITH_CRC_ERROR = 0xB, ///> The command was aborted with a CRC error
	USBI3C_FAILED_GENERAL_ERROR_1 = 0xC,		    ///> Implementation specific transfer type error
	USBI3C_FAILED_GENERAL_ERROR_2 = 0xD,		    ///> Implementation specific transfer type error
	USBI3C_FAILED_GENERAL_ERROR_3 = 0xE,		    ///> Implementation specific transfer type error
	USBI3C_FAILED_GENERAL_ERROR_4 = 0xF		    ///> Implementation specific transfer type error
};

#endif // __libusbi3c_commands_h__
