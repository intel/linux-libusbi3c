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
#include <time.h>

#include "usbi3c.h"

void on_address_change(uint8_t old_address, uint8_t new_address, enum usbi3c_address_change_status request_status, void *user_data)
{
	*(int *)user_data = request_status;
}

int main(int argc, char *argv[])
{
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **devices = NULL;
	struct usbi3c_device *usbi3c_dev = NULL;
	time_t current_time = 0;
	time_t initial_time = 0;
	const int TIMEOUT = 60;
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;
	const int DEVICE_ADDRESS = 100;
	const int NEW_DEVICE_ADDRESS = 200;
	const int UNDEFINED = 0x3;
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

	/* only I3C target devices use dynamic addresses and can have it changed */
	if (usbi3c_get_target_type(usbi3c_dev, DEVICE_ADDRESS) != USBI3C_I3C_DEVICE) {
		goto UNREF_AND_EXIT;
	}

	/* this function submits the request but the actual address change is performed
	 * asynchronously, so we need to provide a callback */
	int address_change_status = UNDEFINED;
	if (usbi3c_change_i3c_device_address(
		usbi3c_dev,
		DEVICE_ADDRESS,
		NEW_DEVICE_ADDRESS,
		on_address_change,
		&address_change_status) < 0) {
		goto UNREF_AND_EXIT;
	}

	initial_time = time(NULL);
	while (address_change_status == UNDEFINED || (current_time > initial_time + TIMEOUT)) {
		/* wait until the adders change executes */
		current_time = time(NULL);
	}

	if (address_change_status == USBI3C_ADDRESS_CHANGE_SUCCEEDED) {
		ret = 0;
	}

UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
