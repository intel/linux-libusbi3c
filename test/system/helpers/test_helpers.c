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

#include "test_helpers.h"

/* Test helper to initialize the I3C controller */
struct usbi3c_device *helper_usbi3c_controller_init(void)
{
	struct usbi3c_context *ctx = NULL;
	struct usbi3c_device **devices = NULL;
	struct usbi3c_device *device = NULL;
	int ret = -1;

	/* vendor and product ID of the USB I3C device we want to use */
	const int VENDOR_ID = 32903;
	const int PRODUCT_ID = 4418;

	/* initialize the library */
	ctx = usbi3c_init();
	assert_non_null(ctx);

	/* set the I3C device for I/O operations */
	ret = usbi3c_get_devices(ctx, VENDOR_ID, PRODUCT_ID, &devices);
	assert_int_equal(ret, 1);
	assert_non_null(devices);
	device = usbi3c_ref_device(devices[0]);

	usbi3c_deinit(&ctx);

	/* initialize the device as controller */
	ret = usbi3c_initialize_device(device);
	assert_int_equal(ret, 0);

	usbi3c_free_devices(&devices);
	return device;
}
