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
	const int DEVICE_ADDRESS = 100;
	const int MAX_SIZE = 1000000;
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;
	const int TIMEOUT = 1000;
	const int MAX_ATTEMPTS = 5;
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

	/* set the maximum number of reattempts for trying to resume a stalled request */
	usbi3c_set_request_reattempt_max(usbi3c_dev, MAX_ATTEMPTS);

	/* set the I3C communication mode options */
	usbi3c_set_i3c_mode(usbi3c_dev, USBI3C_I3C_SDR_MODE, USBI3C_I3C_RATE_4_MHZ, 0);

	/* set the timeout of the USB transactions */
	usbi3c_set_timeout(usbi3c_dev, TIMEOUT);

	/* set the target device max ibi payload of one target device */
	if (usbi3c_set_target_device_max_ibi_payload(usbi3c_dev, DEVICE_ADDRESS, MAX_SIZE) < 0) {
		goto UNREF_AND_EXIT;
	}

	/* set the configurable parameters of one target device */
	const int IBIT = 0b100;
	const int CRR = 0b010;
	const int TIR = 0b001;
	if (usbi3c_set_target_device_config(usbi3c_dev, DEVICE_ADDRESS, IBIT | CRR | TIR) < 0) {
		goto UNREF_AND_EXIT;
	}

	ret = 0;

UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
