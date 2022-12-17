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

#include <stdio.h>

#include "usbi3c.h"

int main(int argc, char *argv[])
{
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **devices = NULL;
	struct usbi3c_device *usbi3c_dev = NULL;
	struct usbi3c_response *response = NULL;
	struct list *responses = NULL;
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;
	const int DEVICE_ADDRESS = 100;
	const int TIMEOUT = 10;
	int ret = -1;

	/* initialize the library */
	ctx = usbi3c_init();
	if (!ctx) {
		return -1;
	}

	/* set the I3C device for I/O operations */
	if (usbi3c_get_devices(ctx, VENDOR_ID, PRODUCT_ID, &devices) < 0) {
		goto EXIT;
	}
	usbi3c_dev = usbi3c_ref_device(devices[0]);
	usbi3c_free_devices(&devices);

	if (usbi3c_get_device_role(usbi3c_dev) != USBI3C_PRIMARY_CONTROLLER_ROLE) {
		goto UNREF_AND_EXIT;
	}

	/* initialize the device */
	if (usbi3c_initialize_device(usbi3c_dev) < 0 && !usbi3c_device_is_active_controller(usbi3c_dev)) {
		goto UNREF_AND_EXIT;
	}

	/* read 1KB of data from the target device with address DEVICE_ADDRESS */
	usbi3c_enqueue_command(
	    usbi3c_dev,
	    DEVICE_ADDRESS,
	    USBI3C_READ,
	    USBI3C_TERMINATE_ON_ANY_ERROR,
	    1024,
	    NULL,
	    NULL,
	    NULL);

	responses = usbi3c_send_commands(
	    usbi3c_dev,
	    USBI3C_NOT_DEPENDENT_ON_PREVIOUS,
	    TIMEOUT);
	if (!responses) {
		goto UNREF_AND_EXIT;
	}

	response = (struct usbi3c_response *)responses->data;
	if (response && response->attempted == USBI3C_COMMAND_ATTEMPTED && response->error_status == USBI3C_SUCCEEDED) {
		ret = 0;
	}

	usbi3c_free_responses(&responses);
UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
