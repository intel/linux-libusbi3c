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

#ifndef __USB_I_H__
#define __USB_I_H__

#include <pthread.h>
#include <stdlib.h>

#include <libusb.h>

#include "list.h"

#define UNLIMITED_TIMEOUT 0

/** Transfers intended for non-isochronous endpoints (e.g. control, bulk,
 * interrupt) should specify an iso_packets count of zero */
#define NON_ISOCHRONOUS 0

/** Timeout (in milliseconds) that a request should wait
 * before giving up due to no response being received */
#define DEFAULT_REQUEST_TIMEOUT 1000

/** Default USB I3C device interface index */
#define USBI3C_INTERFACE_INDEX 0x0

/** Default USB I3C endpoints */
#define USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX 0x00
#define USBI3C_BULK_TRANSFER_ENDPOINT_INDEX 0x02
#define USBI3C_INTERRUPT_ENDPOINT_INDEX 0x03

/* USB functions */
typedef void (*interrupt_dispatcher_fn)(void *interrupt_context, uint8_t *buffer, uint16_t len);
typedef void (*control_transfer_fn)(void *user_context, unsigned char *buffer, uint16_t len);
typedef void (*bulk_transfer_dispatcher_fn)(void *context, unsigned char *buffer, uint32_t buffer_size);
typedef void (*bulk_transfer_completion_fn_t)(void *data);

/**
 * @brief Enumeration of USB I3C device class-specific request codes.
 *
 * These are the supported class-specific requests. Most of these are applicable
 * only when the I3C device is the active I3C controller.
 */
enum i3c_class_request {
	CLEAR_FEATURE = 0x01,		    ///< disable features defined by the value of the "Selector"
	SET_FEATURE = 0x03,		    ///< enable features defined by the value of the "Selector"
	GET_I3C_CAPABILITY = 0x04,	    ///< read the capability of the I3C device in the USB device and target devices
	INITIALIZE_I3C_BUS = 0x05,	    ///< initialize the I3C bus (used if the Host determines the role of the device as I3C Controller)
	GET_TARGET_DEVICE_TABLE = 0x06,	    ///< get the target device table from the I3C function
	SET_TARGET_DEVICE_CONFIG = 0x07,    ///< set the configurable parameters of one or more target devices
	CHANGE_DYNAMIC_ADDRESS = 0x08,	    ///< change a previously assigned dynamic address of one or more I3C target devices
	GET_ADDRESS_CHANGE_RESULT = 0x09,   ///< get the new address after a CHANGE_DYNAMIC_ADDRESS
	GET_BUFFER_AVAILABLE = 0xA,	    ///< get the size of buffer available in the I3C function
	CANCEL_OR_RESUME_BULK_REQUEST = 0xB ///< handle command execution in bulk request transfers after a stalled command
};

/**
 * @brief Structure representing the criteria to use to search for USB devices.
 *
 * Information in this structure is related to the USB device descriptor.
 */
struct usb_search_criteria {
	uint8_t dev_class;   ///< Device class used to identify a device’s functionality
	uint16_t vendor_id;  ///< Number assigned to each company producing USB-based devices
	uint16_t product_id; ///< Number assigned by the manufacturer to identify a specific product
};

struct usb_context;

/**
 * @brief Structure that represent a USB device.
 *
 * This structure is used to represent a USB device.
 */
struct usb_device {
	uint16_t idVendor;  ///< Vendor ID
	uint16_t idProduct; ///< Product ID
	int ref_count;	    ///< Reference count.
};

int usb_context_init(struct usb_context **out_usb_ctx);
int usb_find_devices(struct usb_context *usb_ctx, const struct usb_search_criteria *criteria, struct usb_device ***out_usb_devices);
void usb_context_deinit(struct usb_context *usb_ctx);

int usb_device_init(struct usb_device *usb_dev);
void usb_device_deinit(struct usb_device *usb_dev);
int usb_device_is_initialized(struct usb_device *usb_dev);
int usb_input_control_transfer(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size);
int usb_output_control_transfer(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size);
int usb_input_control_transfer_async(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, control_transfer_fn callback, void *user_context);
int usb_output_control_transfer_async(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size, control_transfer_fn callback, void *user_context);
int usb_input_bulk_transfer(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size);
int usb_output_bulk_transfer(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size);
int usb_input_bulk_transfer_polling(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size, bulk_transfer_dispatcher_fn bulk_transfer_dispatcher);
int usb_input_bulk_transfer_polling_status(struct usb_device *usb_dev);
int usb_get_errno(struct usb_device *usb_dev);
unsigned int usb_set_timeout(struct usb_device *usb_dev, unsigned int timeout);
int usb_get_timeout(struct usb_device *usb_dev);
void usb_set_interrupt_context(struct usb_device *usb_dev, void *interrupt_context);
void *usb_get_interrupt_context(struct usb_device *usb_dev);
void usb_set_interrupt_buffer_length(struct usb_device *usb_dev, int buffer_length);
int usb_interrupt_init(struct usb_device *usb_dev, interrupt_dispatcher_fn dispatcher);
void usb_wait_for_next_event(struct usb_device *usb_dev);
void usb_set_bulk_transfer_context(struct usb_device *usb_dev, void *bulk_transfer_context);
int usb_get_max_bulk_response_buffer_size(struct usb_device *usb_dev);
uint32_t usb_bulk_transfer_response_buffer_init(struct usb_device *usb_dev, unsigned char **buffer);
struct usb_device *usb_device_ref(struct usb_device *usb_dev);
void usb_device_unref(struct usb_device *usb_dev);
void usb_free_devices(struct usb_device **usb_devices, int num_devices);

#endif /* end of include guard: __USB_I_H__ */
