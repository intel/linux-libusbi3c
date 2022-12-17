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

	/* enable the hot-join feature */
	if (usbi3c_enable_hot_join(usbi3c_dev) < 0) {
		goto UNREF_AND_EXIT;
	}

	/* enable regular IBIs in the I3C bus */
	if (usbi3c_enable_regular_ibi(usbi3c_dev) < 0) {
		goto UNREF_AND_EXIT;
	}

	/* disable the ability to handoff the I3C controller role */
	if (usbi3c_disable_i3c_controller_role_handoff(usbi3c_dev) < 0) {
		goto UNREF_AND_EXIT;
	}

	/* disable the remote wake from a hot join request feature */
	if (usbi3c_disable_hot_join_wake(usbi3c_dev) < 0) {
		goto UNREF_AND_EXIT;
	}

	ret = 0;

UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
