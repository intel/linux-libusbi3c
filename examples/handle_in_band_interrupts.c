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

void on_ibi_cb(uint8_t report, struct usbi3c_ibi *descriptor, uint8_t *data, size_t size, void *user_data)
{
	*(int *)user_data = report;
}

int main(int argc, char *argv[])
{
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **devices = NULL;
	struct usbi3c_device *usbi3c_dev = NULL;
	int report = 0;
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;
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

	/* assign a callback function that will be triggered every time an IBI is received */
	usbi3c_on_ibi(usbi3c_dev, on_ibi_cb, &report);

	// do something ...

UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
