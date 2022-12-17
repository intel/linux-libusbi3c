
/***************************************************************************
  usb.c  -  USB interaction
  -------------------
  copyright            : (C) 2021 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

#include <libusb.h>
#include <pthread.h>
#include <string.h>

#include "common_i.h"
#include "usb_i.h"

const int USBI3C_DeviceClass = 0x3C;

/**
 * @brief Structure to manage USB devices.
 *
 * This structure is used to manage USB devices.
 */
struct usb_context {
	struct libusb_context *libusb_context; ///< the libusb session
#define EVENT_THREAD_UNINITIALIZED 0x0
#define EVENT_THREAD_MUTEX_START 0x1
#define EVENT_THREAD_RUNNING 0x2
	uint8_t event_thread_status; ///< variable to handle thread status
	pthread_t event_thread;	     ///< thread to check for USB events
	pthread_mutex_t event_mutex; ///< mutex for main USB event thread
};

/**
 * @brief Structure to describe internal implementation of an USB device
 *
 * This structure is used to describe an USB device. It is used to store
 * information about the device and to store the libusb device handle.
 */
struct priv_usb_device {
	struct usb_device usb_dev;			      ///< public USB device
	struct libusb_device *libusb_device;		      ///< libusb device
	struct libusb_device_handle *handle;		      ///< libusb device handle
	struct usb_context *usb_ctx;			      ///< USB context to handle events
	unsigned int timeout;				      ///< timeout in milliseconds for usb transactions
	int libusb_errno;				      ///< error code libusb would return on failure
	struct libusb_transfer *interrupt_transfer;	      ///< USB interrupt transfer info
	interrupt_dispatcher_fn interrupt_dispatcher;	      ///< function to handle interrupts
	void *interrupt_context;			      ///< context to share with interrupt dispatcher
	unsigned char *interrupt_buffer;		      ///< interrupt data buffer
	unsigned int interrupt_buffer_length;		      ///< interrupt data buffer length
	struct libusb_transfer *bulk_transfer;		      ///< transfer entity for async bulk transfers
	bulk_transfer_dispatcher_fn bulk_transfer_dispatcher; ///< function to handle bulk response transfers
	unsigned char *bulk_transfer_buffer;		      ///< bulk transfer buffer
	void *bulk_transfer_context;			      ///< context to share with bulk transfer dispatcher
	uint8_t stop_events;				      ///< flag to stop event thread
};

/**
 * @brief Structure to handle Control transfer context
 *
 * This structure is used to handle Control transfer context.
 */
struct async_control_transfer_context {
	struct priv_usb_device *priv_usb_dev; ///< USB device
	control_transfer_fn callback;	      ///< callback function
	void *user_context;		      ///< user context
};

// Function to stop the event thread
static void set_event_thread_stop(struct usb_context *usb_ctx)
{
	pthread_mutex_lock(&usb_ctx->event_mutex);
	usb_ctx->event_thread_status &= ~EVENT_THREAD_RUNNING;
	pthread_mutex_unlock(&usb_ctx->event_mutex);
}

// Function to check if the event thread is running
static int get_event_thread_status(struct usb_context *usb_ctx)
{
	int status;
	pthread_mutex_lock(&usb_ctx->event_mutex);
	status = usb_ctx->event_thread_status;
	pthread_mutex_unlock(&usb_ctx->event_mutex);
	return status;
}

// Function to start the event thread
static void *event_thread_handler(void *arg)
{
	struct usb_context *usb_ctx = (struct usb_context *)arg;
	while (get_event_thread_status(usb_ctx) & EVENT_THREAD_RUNNING)
		libusb_handle_events(usb_ctx->libusb_context);
	return NULL;
}

/**
 * @brief Initialize USB context
 *
 * This function initializes the USB context.
 * @param[out] out_usb_ctx USB context
 * @return 0 on success, negative error code on failure
 */
int usb_context_init(struct usb_context **out_usb_ctx)
{
	struct usb_context *usb_ctx = malloc_or_die(sizeof(struct usb_context));
	int err = 0;

	usb_ctx->event_thread_status = EVENT_THREAD_UNINITIALIZED;

	if ((err = libusb_init(&usb_ctx->libusb_context)) < 0) {
		DEBUG_PRINT("libusb_init(): %s\n",
			    libusb_error_name(err));
		goto CLEAN_AND_EXIT;
	}

	usb_ctx->event_thread_status |= EVENT_THREAD_MUTEX_START;
	if ((err = pthread_mutex_init(&usb_ctx->event_mutex, NULL))) {
		usb_ctx->event_thread_status &= ~EVENT_THREAD_MUTEX_START;
		DEBUG_PRINT("pthread_mutex_init(): %s\n",
			    strerror(err));
		goto CLEAN_AND_EXIT;
	}

	usb_ctx->event_thread_status |= EVENT_THREAD_RUNNING;
	if ((err = pthread_create(&usb_ctx->event_thread, NULL, &event_thread_handler, usb_ctx))) {
		usb_ctx->event_thread_status &= ~EVENT_THREAD_RUNNING;
		DEBUG_PRINT("pthread_create(): %s\n",
			    strerror(err));
		goto CLEAN_AND_EXIT;
	}

	*out_usb_ctx = usb_ctx;
	return 0;
CLEAN_AND_EXIT:
	usb_context_deinit(usb_ctx);
	return err;
}

/**
 * @brief Deinitialize USB context
 *
 * This function deinitializes the USB context.
 * @param[in] usb_ctx USB context
 */
void usb_context_deinit(struct usb_context *usb_ctx)
{
	if (get_event_thread_status(usb_ctx) & EVENT_THREAD_RUNNING) {
		set_event_thread_stop(usb_ctx);
		pthread_join(usb_ctx->event_thread, NULL);
	}

	if (get_event_thread_status(usb_ctx) & EVENT_THREAD_MUTEX_START) {
		pthread_mutex_destroy(&usb_ctx->event_mutex);
	}
	if (usb_ctx->libusb_context) {
		libusb_exit(usb_ctx->libusb_context);
		usb_ctx->libusb_context = NULL;
	}
	FREE(usb_ctx);
}

/**
 * @brief Compares a USB descriptor against a specific criteria.
 *
 * The criteria can include one or more of the following:
 *  - device class
 *  - vendor ID
 *  - product ID
 *
 * @param[in] desc the libusb_device_descriptor to compare
 * @param[in] criteria the usbi3c criteria to compare against
 * @return 1 if the descriptor matches the criteria, 0 if it does not
 */
static int usb_match_criteria(struct libusb_device_descriptor *desc, const struct usb_search_criteria *criteria)
{
	int final_cond = 1;

	if (criteria == NULL) {
		return 1;
	}
	if (criteria->product_id) {
		final_cond = final_cond && desc->idProduct == criteria->product_id;
	}
	if (criteria->vendor_id) {
		final_cond = final_cond && desc->idVendor == criteria->vendor_id;
	}
	if (criteria->dev_class) {
		final_cond = final_cond && desc->bDeviceClass == criteria->dev_class;
	}

	return final_cond;
}

// function to create a usb_device structure from a libusb_device
static struct usb_device *create_usb_device_from_libusb_device(struct usb_context *usb_ctx, struct libusb_device_descriptor *desc, struct libusb_device *device)
{
	struct priv_usb_device *priv_usb_dev = malloc_or_die(sizeof(struct priv_usb_device));
	priv_usb_dev->libusb_device = libusb_ref_device(device);
	priv_usb_dev->handle = NULL;
	priv_usb_dev->usb_ctx = usb_ctx;
	priv_usb_dev->usb_dev.idVendor = desc->idVendor;
	priv_usb_dev->usb_dev.idProduct = desc->idProduct;
	priv_usb_dev->usb_dev.ref_count = 1;
	priv_usb_dev->stop_events = 0;
	return &priv_usb_dev->usb_dev;
}

/**
 * @brief Function to search for USB devices.
 *
 * This function is used to search for USB devices.
 *
 * @param[in] usb_ctx USB context.
 * @param[in] criteria USB search criteria.
 * @param[out] out_usb_devices List of USB devices found.
 * @return number of devices found, or -1 on error.
 */
int usb_find_devices(struct usb_context *usb_ctx, const struct usb_search_criteria *criteria, struct usb_device ***out_usb_devices)
{
	int res = 0;
	int device_number = 0;
	int device_count = 0;
	struct libusb_device **libusb_devices = NULL;
	struct usb_device **usb_devices = NULL;

	// get the list of USB devices currently attached to the system
	if ((device_number = libusb_get_device_list(usb_ctx->libusb_context, &libusb_devices)) < 0) {
		DEBUG_PRINT("libusb_get_device_list(): %s\n",
			    libusb_error_name(device_number));
		return device_number;
	}

	if (device_number == 0) {
		DEBUG_PRINT("No USB devices found\n");
		return 0;
	}

	// allocate memory for the list of USB devices
	usb_devices = malloc_or_die(sizeof(struct usb_device *) * device_number);

	// create a list of attached USB devices that match the provided criteria
	for (int i = 0; i < device_number; i++) {
		struct libusb_device_descriptor desc;
		if ((res = libusb_get_device_descriptor(libusb_devices[i], &desc)) < 0) {
			DEBUG_PRINT("libusb_get_device_descriptor(): %s\n",
				    libusb_error_name(res));
			goto FREE_AND_EXIT;
		}
		if (usb_match_criteria(&desc, criteria)) {
			usb_devices[device_count++] = create_usb_device_from_libusb_device(usb_ctx, &desc, libusb_devices[i]);
		}
	}

	if (device_count == 0) {
		DEBUG_PRINT("No matching USB devices found\n");
		res = 0;
		goto FREE_AND_EXIT;
	}

	usb_devices = realloc_or_die(usb_devices, sizeof(struct usb_device *) * device_count);
	*out_usb_devices = usb_devices;

	// free the list of USB devices
	libusb_free_device_list(libusb_devices, 1);

	return device_count;
FREE_AND_EXIT:
	libusb_free_device_list(libusb_devices, 1);
	FREE(usb_devices);
	return res;
}

/**
 * @brief Function to create a reference to a USB device.
 *
 * This function is used to create a reference to a USB device.
 *
 * @param[in] usb_dev USB device.
 * @return a pointer to the USB device.
 */
struct usb_device *usb_device_ref(struct usb_device *usb_dev)
{
	usb_dev->ref_count++;
	return usb_dev;
}

/**
 * @brief Function to remove a reference to a USB device.
 *
 * This function is used to remove a reference to a USB device.
 *
 * @param[in] usb_dev USB device.
 */
void usb_device_unref(struct usb_device *usb_dev)
{
	usb_dev->ref_count--;
}

/**
 * @brief Function to free a list of USB devices.
 *
 * This function is used to free a list of USB devices.
 *
 * @param[in] usb_devices List of USB devices.
 * @param[in] num_devices Number of USB devices in the list.
 */
void usb_free_devices(struct usb_device **usb_devices, int num_devices)
{
	for (int i = 0; i < num_devices; i++) {
		usb_device_deinit(usb_devices[i]);
	}
	FREE(usb_devices);
}

// function to claim a USB interface and detach the kernel driver if needed
static int set_interface(struct priv_usb_device *priv_usb_dev)
{
	int ret = 0;

	if (libusb_kernel_driver_active(priv_usb_dev->handle, USBI3C_INTERFACE_INDEX) == 1) {
		if ((ret = libusb_detach_kernel_driver(priv_usb_dev->handle, USBI3C_INTERFACE_INDEX)) != 0) {
			libusb_close(priv_usb_dev->handle);
			priv_usb_dev->handle = NULL;
			return ret;
		}
	}
	if ((ret = libusb_claim_interface(priv_usb_dev->handle, USBI3C_INTERFACE_INDEX)) != 0) {
		libusb_close(priv_usb_dev->handle);
		priv_usb_dev->handle = NULL;
		return ret;
	}
	return 0;
}

// function to open a USB device
static int open_device(struct priv_usb_device *priv_usb_dev)
{
	struct libusb_device_handle *device_handler = NULL;
	int res = 0;

	if ((res = libusb_open(priv_usb_dev->libusb_device, &device_handler)) < 0) {
		DEBUG_PRINT("libusb_open(): error opening device %s\n",
			    libusb_error_name(res));
		return res;
	}
	priv_usb_dev->handle = device_handler;
	return 0;
}

/**
 * @brief Function to initialize an USB device.
 *
 * This function is used to initialize an USB device.
 *
 * @param[in] usb_dev USB device.
 * @return 0 on success, or -1 on error.
 */
int usb_device_init(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	int ret = 0;
	if ((ret = open_device(priv_usb_dev)) < 0) {
		return ret;
	}
	if ((ret = set_interface(priv_usb_dev)) < 0) {
		return ret;
	}

	priv_usb_dev->libusb_errno = 0;
	priv_usb_dev->timeout = DEFAULT_REQUEST_TIMEOUT;
	priv_usb_dev->interrupt_transfer = libusb_alloc_transfer(NON_ISOCHRONOUS);
	if (priv_usb_dev->interrupt_transfer == NULL) {
		DEBUG_PRINT("libusb_alloc_transfer() failed to allocate a transfer for interrupts\n");
		ret = -1;
		goto CLOSE_AND_EXIT;
	}
	priv_usb_dev->interrupt_dispatcher = NULL;
	priv_usb_dev->interrupt_context = NULL;
	priv_usb_dev->interrupt_buffer = NULL;
	priv_usb_dev->interrupt_buffer_length = 0;
	priv_usb_dev->bulk_transfer = libusb_alloc_transfer(NON_ISOCHRONOUS);
	if (priv_usb_dev->bulk_transfer == NULL) {
		DEBUG_PRINT("libusb_alloc_transfer() failed to allocate a transfer for bulk\n");
		libusb_free_transfer(priv_usb_dev->interrupt_transfer);
		ret = -1;
		goto CLOSE_AND_EXIT;
	}
	priv_usb_dev->bulk_transfer_dispatcher = NULL;
	priv_usb_dev->bulk_transfer_buffer = NULL;
	priv_usb_dev->bulk_transfer_context = NULL;

	return ret;
CLOSE_AND_EXIT:
	libusb_release_interface(priv_usb_dev->handle, USBI3C_INTERFACE_INDEX);
	libusb_close(priv_usb_dev->handle);
	priv_usb_dev->handle = NULL;
	return ret;
}

/**
 * @brief Function to deinitialize an USB device.
 *
 * This function is used to deinitialize an USB device.
 *
 * @param[in] usb_dev USB device.
 */
void usb_device_deinit(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);

	usb_device_unref(usb_dev);
	if (usb_dev->ref_count > 0) {
		return;
	}

	priv_usb_dev->stop_events = 1;

	if (priv_usb_dev->handle) {
		libusb_release_interface(priv_usb_dev->handle, USBI3C_INTERFACE_INDEX);
		libusb_close(priv_usb_dev->handle);
		priv_usb_dev->handle = NULL;
	}
	if (priv_usb_dev->libusb_device) {
		libusb_unref_device(priv_usb_dev->libusb_device);
		priv_usb_dev->libusb_device = NULL;
	}
	if (priv_usb_dev->interrupt_transfer) {
		libusb_free_transfer(priv_usb_dev->interrupt_transfer);
	}

	if (priv_usb_dev->interrupt_buffer) {
		FREE(priv_usb_dev->interrupt_buffer);
	}

	if (priv_usb_dev->bulk_transfer) {
		libusb_free_transfer(priv_usb_dev->bulk_transfer);
	}

	if (priv_usb_dev->bulk_transfer_buffer) {
		FREE(priv_usb_dev->bulk_transfer_buffer);
	}

	FREE(priv_usb_dev);
}

/* perform a USB control transfer */
static int control_transfer(struct priv_usb_device *priv_usb_dev, enum i3c_class_request request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size, unsigned char endpoint_direction)
{
	int res = 0;
	uint8_t request_type;
	const int CONTROL_REQUEST_ENDPOINT_NUMBER = 0;

	if (priv_usb_dev->handle == NULL) {
		return -1;
	}

	request_type = endpoint_direction | CONTROL_REQUEST_ENDPOINT_NUMBER | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
	res = libusb_control_transfer(priv_usb_dev->handle, request_type, request, value, index, data, data_size, priv_usb_dev->timeout);
	if (res < 0) {
		DEBUG_PRINT("libusb_control_transfer(): %s\n", libusb_error_name(res));
		return res;
	}

	return res;
}

// perform an asynchronous USB control transfer
static void async_control_transfer_handler(struct libusb_transfer *transfer)
{
	struct async_control_transfer_context *async_context = NULL;
	struct priv_usb_device *priv_usb_dev = NULL;
	unsigned char *buffer = NULL;

	async_context = (struct async_control_transfer_context *)transfer->user_data;
	priv_usb_dev = async_context->priv_usb_dev;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		priv_usb_dev->libusb_errno = transfer->status;
		DEBUG_PRINT("Something went wrong, control transfer did not complete\n");
	} else if (transfer->length > 0 && async_context->callback) {
		/* we have a valid control transfer, and control transfers begin with a setup packet,
		 * we don't need to pass the setup packet to the callback function, only the data */
		buffer = LIBUSB_CONTROL_SETUP_SIZE + transfer->buffer;
		async_context->callback(async_context->user_context, buffer, transfer->actual_length);
	}

	FREE(async_context);
	FREE(transfer->buffer);
	libusb_free_transfer(transfer);
}

/**
 * @brief Perform a USB input control transfer asynchronously.
 *
 * @param[in] usb_dev the USB device
 * @param[in] request the I3C device class specific request code
 * @param[in] value the value field for setup packet
 * @param[in] index the index field for the setup packet
 * @param[in] callback the function to call once the asynchronous transaction is completed
 * @param[in] user_context pointer to share with callback function
 * @return 0 if the transfer was received successfully, or -1 otherwise
 */
int usb_input_control_transfer_async(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, control_transfer_fn callback, void *user_context)
{
	const int MAX_TRANSFER_SIZE = 4096;
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	struct async_control_transfer_context *async_context = NULL;
	struct libusb_transfer *control_transfer = NULL;
	uint16_t buffer_length = MAX_TRANSFER_SIZE;
	unsigned char *buffer = NULL;
	uint8_t request_type = LIBUSB_ENDPOINT_IN |
			       USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX |
			       LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
	int ret = 0;

	if (priv_usb_dev->handle == NULL) {
		return -1;
	}

	async_context = malloc_or_die(sizeof(struct async_control_transfer_context));
	async_context->callback = callback;
	async_context->user_context = user_context;
	async_context->priv_usb_dev = priv_usb_dev;

	control_transfer = libusb_alloc_transfer(NON_ISOCHRONOUS);
	buffer = malloc_or_die(buffer_length);

	libusb_fill_control_setup(buffer, request_type, request, value, index, buffer_length);
	libusb_fill_control_transfer(control_transfer,
				     priv_usb_dev->handle,
				     buffer,
				     async_control_transfer_handler,
				     async_context,
				     priv_usb_dev->timeout);
	ret = libusb_submit_transfer(control_transfer);
	if (ret != 0) {
		libusb_free_transfer(control_transfer);
		FREE(async_context);
		FREE(buffer);
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		return ret;
	}

	return 0;
}

/**
 * @brief Perform a USB output control transfer asynchronously.
 *
 * @param[in] usb_dev the USB device
 * @param[in] request the I3C device class specific request code
 * @param[in] value the value field for setup packet
 * @param[in] index the index field for the setup packet
 * @param[in] data a buffer with the data to be sent
 * @param[in] data_size the size of the data to be transmitted
 * @param[in] callback the function to call once the asynchronous transaction is completed
 * @param[in] user_context pointer to share with callback function
 * @return 0 if the transfer was submitted successfully, or -1 otherwise
 */
int usb_output_control_transfer_async(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size, control_transfer_fn callback, void *user_context)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	struct libusb_transfer *control_transfer = NULL;
	struct async_control_transfer_context *async_context = NULL;
	unsigned char *buffer = NULL;
	uint16_t buffer_size = 0;
	uint8_t request_type = 0;
	int ret = -1;

	if (priv_usb_dev->handle == NULL) {
		return -1;
	}

	/* we'll send this context data to the async_control_transfer_handler() function
	 * which will be called when the async control transfer finishes */
	async_context = (struct async_control_transfer_context *)malloc_or_die(sizeof(struct async_control_transfer_context));
	async_context->callback = callback;
	async_context->user_context = user_context;
	async_context->priv_usb_dev = priv_usb_dev;

	request_type = LIBUSB_ENDPOINT_OUT | USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX |
		       LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

	/* allocation */
	control_transfer = libusb_alloc_transfer(NON_ISOCHRONOUS);

	/* all control transfers start with a setup packet that precede the data to be sent
	 * so we need to prepare a new data buffer with this */
	buffer_size = LIBUSB_CONTROL_SETUP_SIZE + data_size;
	buffer = (unsigned char *)malloc_or_die(buffer_size);
	memcpy(buffer + LIBUSB_CONTROL_SETUP_SIZE, data, data_size);
	libusb_fill_control_setup(buffer, request_type, request, value, index, data_size);

	/* filling */
	libusb_fill_control_transfer(control_transfer,
				     priv_usb_dev->handle,
				     buffer,
				     async_control_transfer_handler,
				     async_context,
				     priv_usb_dev->timeout);

	/* submission */
	ret = libusb_submit_transfer(control_transfer);
	if (ret != 0) {
		libusb_free_transfer(control_transfer);
		FREE(async_context);
		FREE(buffer);
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		return ret;
	}

	return 0;
}

/**
 * @brief Perform a USB input control transfer.
 *
 * @param[in] usb_dev the USB device
 * @param[in] request the I3C device class-specific request code
 * @param[in] value the value field for the setup packet
 * @param[in] index the index field for the setup packet
 * @param[out] data a suitably-sized data buffer
 * @param[in] data_size the maximum number of BYTES to receive into the data buffer
 * @return 0 if the data was received successfully, or -1 otherwise
 */
int usb_input_control_transfer(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return control_transfer(priv_usb_dev, request, value, index, data, data_size, LIBUSB_ENDPOINT_IN);
}

/**
 * @brief Perform a USB output control transfer.
 *
 * @param[in] usb_dev the USB device
 * @param[in] request the I3C device class-specific request code
 * @param[in] value the value field for the setup packet
 * @param[in] index the index field for the setup packet
 * @param[in] data a suitably-sized data buffer
 * @param[in] data_size the number of BYTES of data to be sent
 * @return 0 on success, or -1 otherwise
 */
int usb_output_control_transfer(struct usb_device *usb_dev, uint8_t request, uint16_t value, uint16_t index, unsigned char *data, uint16_t data_size)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return control_transfer(priv_usb_dev, request, value, index, data, data_size, LIBUSB_ENDPOINT_OUT);
}

/**
 * @brief Gets the maximum size of the bulk response transfer buffer.
 *
 * @param[in] usb_dev the USB device
 * @return the bulk response transfer buffer size
 */
int usb_get_max_bulk_response_buffer_size(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	int max_packet_size = 0;
	const int MULTIPLE = 1000;

	if (priv_usb_dev->handle == NULL) {
		DEBUG_PRINT("get_max_bulk_response_buffer_size(): device not initialized\n");
		return -1;
	}

	/* Bulk transfers are split into packets (typically small, with a maximum
	 * size of 512 bytes). Overflows can only happen if the final packet in an
	 * incoming data transfer is smaller than the actual packet that the device
	 * wants to transfer. To avoid overflows, the transfer buffer size has to
	 * be a multiple of the endpoint's packet size: therefore, the final packet
	 * will either fill up completely or will be only partially filled. */
	max_packet_size = libusb_get_max_packet_size(priv_usb_dev->libusb_device, USBI3C_BULK_TRANSFER_ENDPOINT_INDEX);
	if (max_packet_size < 0) {
		DEBUG_PRINT("libusb_get_max_packet_size(): %s\n", libusb_error_name(max_packet_size));
		return max_packet_size;
	}

	return max_packet_size * MULTIPLE;
}

/**
 * @brief Initializes a buffer to receive bulk responses from an I3C function.
 *
 * @param[in] usb_dev the USB device
 * @param[out] buffer the buffer to initialize to be used to receive bulk responses
 * @return the size of the initialized buffer
 */
uint32_t usb_bulk_transfer_response_buffer_init(struct usb_device *usb_dev, unsigned char **buffer)
{
	int64_t buffer_size = 0;

	/* We cannot know in advanced how big a bulk response is going to be,
	 * there are 3 different type of bulk responses supported.
	 * The only thing we can know for sure is that there is a bulk response
	 * transfer header of a size of 32 bits, followed by one or many blocks
	 * of a currently unknown size.
	 * We won't know the size of the other blocks until after we have received
	 * the bulk response and have determined its type. And even then, we will
	 * only know the size of the current response block we are reading. So to
	 * get a suitably-sized buffer for the response, and avoid overflows, the
	 * buffer size has to be a multiple of the endpoint's packet size. */
	buffer_size = usb_get_max_bulk_response_buffer_size(usb_dev);
	if (buffer_size <= 0) {
		return 0;
	}

	*buffer = (unsigned char *)malloc_or_die((size_t)buffer_size);
	return buffer_size;
}

/* perform a USB bulk transfer */
static int bulk_transfer(struct priv_usb_device *priv_usb_dev, unsigned char *data, uint32_t data_size, unsigned char endpoint)
{
	int res = 0;
	int transferred = 0;

	if (priv_usb_dev->handle == NULL) {
		return -1;
	}

	/* TODO: we probably want to give users the option to dynamically configure this transfer timeout */
	res = libusb_bulk_transfer(priv_usb_dev->handle, endpoint, data, data_size, &transferred, UNLIMITED_TIMEOUT);
	if (res == 0 && transferred == data_size) {
		/* data transferred correctly */
		return res;
	} else if (res == 0 && transferred != data_size) {
		DEBUG_PRINT("libusb_bulk_transfer(): different data size transferred (%d) vs expected (%d)\n", transferred, data_size);
		return -1;
	} else {
		DEBUG_PRINT("libusb_bulk_transfer(): %s\n", libusb_error_name(res));
		return res;
	}
}

/**
 * @brief Perform a USB input bulk transfer.
 *
 * @param[in] usb_dev the USB device with an open and claimed device
 * @param[out] data a suitably-sized data buffer for the data to be received
 * @param[in] data_size the maximum number of BYTES to receive into the data buffer
 * @return 0 if the data was received successfully, or -1 otherwise
 */
int usb_input_bulk_transfer(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return bulk_transfer(priv_usb_dev, data, data_size, USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_IN);
}

/**
 * @brief Perform a USB output bulk transfer.
 *
 * @param[in] usb_dev the USB device with an open and claimed device
 * @param[in] data the data buffer to to be transferred
 * @param[in] data_size the number of BYTES of data to be sent
 * @return 0 if the data was transferred successfully, or -1 otherwise
 */
int usb_output_bulk_transfer(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return bulk_transfer(priv_usb_dev, data, data_size, USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_OUT);
}

/**
 * @brief Submits a USB input bulk transfer.
 *
 * There may be a long delay between starting a transfer and completion, however
 * the asynchronous submission function is non-blocking so will return control to
 * your application during that potentially long delay. When the transfer completes
 * it will cause the user-specified bulk_transfer_completion_cb() callback function
 * to be invoked, cb_data will be made available to the callback function.
 *
 * @param[in] priv_usb_dev the usb session with an open and claimed device
 * @param[in] data a suitably-sized data buffer for the data to be received
 * @param[in] data_size the maximum number of BYTES to receive into the data buffer
 * @param[in] bulk_transfer_completion_cb the callback function to be run at the bulk transfer completion
 * @param[in] cb_data the data that will be passed to the callback function
 * @return 0 if the transfer was submitted successfully, or -1 otherwise
 */
static int input_bulk_transfer_async(struct priv_usb_device *priv_usb_dev, unsigned char *data, uint32_t data_size, bulk_transfer_completion_fn_t bulk_transfer_completion_cb, void *cb_data)
{
	int ret = -1;

	libusb_fill_bulk_transfer(priv_usb_dev->bulk_transfer,
				  priv_usb_dev->handle,
				  USBI3C_BULK_TRANSFER_ENDPOINT_INDEX | LIBUSB_ENDPOINT_IN,
				  data,
				  data_size,
				  (libusb_transfer_cb_fn)bulk_transfer_completion_cb,
				  cb_data,
				  priv_usb_dev->timeout);

	/* fire off the I/O request in the background */
	ret = libusb_submit_transfer(priv_usb_dev->bulk_transfer);
	if (ret != 0) {
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		priv_usb_dev->bulk_transfer_dispatcher = NULL;
		return ret;
	}

	return 0;
}

/**
 * @brief Asynchronous transfer callback function that resubmits the transfer.
 *
 * In order to create a polling mechanism for continuously checking for responses
 * from a USB device, we need to resubmit an input bulk transfer as soon as one
 * completes therefore creating a polling mechanism.
 *
 * @note: This function should only be used as callback by libusb when a bulk
 * transfer completes.
 *
 * @param[in] transfer the libusb bulk transfer entity.
 */
static void input_bulk_transfer_polling_cb(void *data)
{
	struct libusb_transfer *transfer = (struct libusb_transfer *)data;
	struct priv_usb_device *priv_usb_dev = (struct priv_usb_device *)transfer->user_data;
	int ret = -1;

	if (priv_usb_dev->stop_events) {
		return;
	}

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->actual_length > 0 && priv_usb_dev->bulk_transfer_dispatcher) {
			/* we have a valid bulk transfer */
			priv_usb_dev->bulk_transfer_dispatcher(priv_usb_dev->bulk_transfer_context, transfer->buffer, transfer->actual_length);
		}
	} else if (transfer->status != LIBUSB_TRANSFER_TIMED_OUT) {
		priv_usb_dev->libusb_errno = transfer->status;
		DEBUG_PRINT("Input bulk transfer failed with status code %d\n", transfer->status);
	}

	/* we just received an input bulk transfer, let's fire off another one */
	ret = libusb_submit_transfer(transfer);
	if (ret != 0) {
		priv_usb_dev->libusb_errno = ret;
		priv_usb_dev->bulk_transfer_dispatcher = NULL;
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		DEBUG_PRINT("Something went wrong, the input bulk transfer polling has been stopped.\n");
	}
}

/**
 * @brief Initiates a polling for input bulk transfers.
 *
 * @param[in] usb_dev the USB device with an open and claimed device
 * @param[in] data a suitably-sized data buffer for the data to be received
 * @param[in] data_size the maximum number of BYTES to receive into the data buffer
 * @param[in] bulk_transfer_dispatcher the function to call when we have a valid input bulk transfer
 * @return 0 if the polling was started successfully, or -1 otherwise
 */
int usb_input_bulk_transfer_polling(struct usb_device *usb_dev, unsigned char *data, uint32_t data_size, bulk_transfer_dispatcher_fn bulk_transfer_dispatcher)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	int ret = -1;

	if (priv_usb_dev->handle == NULL) {
		return -1;
	}

	if (data == NULL || data_size == 0) {
		DEBUG_PRINT("No data buffer provided for the bulk transfer\n");
		return -1;
	}

	if (bulk_transfer_dispatcher == NULL) {
		DEBUG_PRINT("No dispatcher callback provided\n");
		return -1;
	}

	priv_usb_dev->bulk_transfer_buffer = data;
	priv_usb_dev->bulk_transfer_dispatcher = bulk_transfer_dispatcher;

	ret = input_bulk_transfer_async(priv_usb_dev, data, data_size, input_bulk_transfer_polling_cb, priv_usb_dev);
	if (ret != 0) {
		DEBUG_PRINT("Failed to start polling for input bulk transfers\n");
		priv_usb_dev->bulk_transfer_dispatcher = NULL;
	}

	return ret;
}

/**
 * @brief Checks the status of the bulk response transfer polling.
 *
 * @param[in] usb_dev the USB device with an open and claimed device
 * @return 1 if the polling has been initiated, 0 otherwise
 */
int usb_input_bulk_transfer_polling_status(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return (priv_usb_dev->bulk_transfer_buffer && priv_usb_dev->bulk_transfer_dispatcher && priv_usb_dev->bulk_transfer);
}

/**
 * @brief sets the bulk transfer context.
 *
 * This context will be shared with the usb_input_bulk_transfer_polling_cb() callback
 * function.
 *
 * @param[in] usb_dev the USB device
 * @param[in] bulk_transfer_context pointer to data to be used in usb_input_bulk_transfer_polling_cb()
 */
void usb_set_bulk_transfer_context(struct usb_device *usb_dev, void *bulk_transfer_context)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	priv_usb_dev->bulk_transfer_context = bulk_transfer_context;
}

/**
 * @brief Function to get USB error code that happen during an asynchronous transfer.
 *
 * @param[in] usb_dev the USB device
 * @return the error code
 */
int usb_get_errno(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	int error_number = priv_usb_dev->libusb_errno;
	/* we are reading the error value, we can reset the value now */
	priv_usb_dev->libusb_errno = 0;
	return error_number;
}

/**
 * @brief Set USB transaction timeout.
 *
 * @param[in] usb_dev the USB device
 * @param[in] timeout the maximum time in milliseconds to wait for a transaction
 * @return previous timeout value
 */
unsigned int usb_set_timeout(struct usb_device *usb_dev, unsigned int timeout)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	unsigned int previous = priv_usb_dev->timeout;
	priv_usb_dev->timeout = timeout;
	return previous;
}

/**
 * @brief Gets the USB transaction timeout.
 *
 * @param[in] usb_dev the USB device
 * @return the timeout value, or -1 on failure
 */
int usb_get_timeout(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	if (priv_usb_dev == NULL) {
		return -1;
	}
	return priv_usb_dev->timeout;
}

/**
 * @brief set interrupt context.
 *
 *  Context to share every interrupt triggered.
 *
 * @param[in] usb_dev the USB device
 * @param[in] interrupt_context pointer to data to be used in interrupts
 */
void usb_set_interrupt_context(struct usb_device *usb_dev, void *interrupt_context)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	priv_usb_dev->interrupt_context = interrupt_context;
}

/**
 * @brief Get interrupt context.
 *
 *  Get pre-setted interrupt context
 *
 * @param[in] usb_dev the USB device
 * @return pointer to interrupt context
 */
void *usb_get_interrupt_context(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return priv_usb_dev->interrupt_context;
}

/**
 * @brief Set USB interrupt buffer length.
 *
 *  If USB needs a buffer for interrupt transaction this function set the buffer length
 *
 * @param[in] usb_dev the USB device
 * @param[in] buffer_length interrupt buffer length
 */
void usb_set_interrupt_buffer_length(struct usb_device *usb_dev, int buffer_length)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	priv_usb_dev->interrupt_buffer_length = buffer_length;
}

/**
 * @brief USB interrupt handler.
 *
 *  This function is called every time an interrupt is triggered,
 *  it will handle USB error and if everything is right will execute
 *  interrupt dispatcher previously added in usb_interrupt_init function.
 *
 * @param[in] transfer libusb transfer structure.
 */
static void handle_interrupt(struct libusb_transfer *transfer)
{
	struct priv_usb_device *priv_usb_dev = (struct priv_usb_device *)transfer->user_data;
	int ret = 0;

	if (priv_usb_dev->stop_events) {
		return;
	}
	if (transfer->actual_length > 0) {
		if (priv_usb_dev->interrupt_dispatcher) {
			void *interrupt_context = usb_get_interrupt_context(&priv_usb_dev->usb_dev);
			priv_usb_dev->interrupt_dispatcher(interrupt_context, transfer->buffer, transfer->actual_length);
		}
	} else if (transfer->status != LIBUSB_TRANSFER_TIMED_OUT &&
		   transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		priv_usb_dev->libusb_errno = transfer->status;
		return;
	}
	if ((ret = libusb_submit_transfer(transfer)) != 0) {
		priv_usb_dev->libusb_errno = ret;
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		return;
	}
}

/**
 * @brief Initialize interrupt handler.
 *
 *  This function starts listening interrupt endpoint asynchronously and
 *  set interrupt dispatcher function that will be executed in handle_interrupt.
 *
 * @param[in] usb_dev the USB device
 * @param[in] dispatcher function to call when an interrupt is triggered
 * @return 0 if the interrupt handler started correctly, or -1 otherwise
 */
int usb_interrupt_init(struct usb_device *usb_dev, interrupt_dispatcher_fn dispatcher)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	int ret = 0;
	if (priv_usb_dev->handle == NULL) {
		return -1;
	}
	priv_usb_dev->interrupt_dispatcher = dispatcher;
	if (priv_usb_dev->interrupt_buffer_length) {
		priv_usb_dev->interrupt_buffer = malloc_or_die(sizeof(unsigned char) * priv_usb_dev->interrupt_buffer_length);
	}
	libusb_fill_interrupt_transfer(priv_usb_dev->interrupt_transfer,
				       priv_usb_dev->handle,
				       LIBUSB_ENDPOINT_IN | USBI3C_INTERRUPT_ENDPOINT_INDEX,
				       priv_usb_dev->interrupt_buffer,
				       priv_usb_dev->interrupt_buffer_length,
				       handle_interrupt,
				       priv_usb_dev,
				       priv_usb_dev->timeout);
	if ((ret = libusb_submit_transfer(priv_usb_dev->interrupt_transfer)) != 0) {
		DEBUG_PRINT("libusb_submit_transfer(): %s\n", libusb_error_name(ret));
		return ret;
	}
	return 0;
}

/**
 * @brief wait for next event.
 *
 *  This function is going to lock current running thread
 *  until any interrupt comes from the interrupt endpoint.
 *
 * @param[in] usb_dev the USB device
 */
void usb_wait_for_next_event(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	struct usb_context *usb_ctx = priv_usb_dev->usb_ctx;

	libusb_lock_event_waiters(usb_ctx->libusb_context);
	libusb_wait_for_event(usb_ctx->libusb_context, NULL);
	libusb_unlock_event_waiters(usb_ctx->libusb_context);
}

/**
 * @brief Function to check if a USB device is initalized.
 *
 * This function is used to get check if an USB device is initalized.
 *
 * @param[in] usb_dev USB device.
 * @return 1 if initalized, 0 if not.
 */
int usb_device_is_initialized(struct usb_device *usb_dev)
{
	struct priv_usb_device *priv_usb_dev = container_of(usb_dev, struct priv_usb_device, usb_dev);
	return priv_usb_dev->handle != NULL;
}
