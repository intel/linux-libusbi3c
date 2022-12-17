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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "usbi3c.h"

struct callback_data {
	int ccc_executed;
	struct usbi3c_response *response;
};

int response_cb(struct usbi3c_response *response, void *user_data)
{
	struct callback_data *cb_data = (struct callback_data *)user_data;

	memcpy(cb_data->response, response, sizeof(struct usbi3c_response));
	if (response->data_length > 0 && response->data) {
		cb_data->response->data = (unsigned char *)calloc(response->data_length, sizeof(unsigned char));
		memcpy(cb_data->response->data, response->data, response->data_length);
	}
	cb_data->ccc_executed = 1;

	return 0;
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
	const int RSTDAA = 0x06;
	struct callback_data cb_data = { 0 };
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

	/* write data_to_write in the target device with address DEVICE_ADDRESS */
	usbi3c_enqueue_ccc(
	    usbi3c_dev,
	    USBI3C_BROADCAST_ADDRESS,
	    USBI3C_WRITE,
	    USBI3C_TERMINATE_ON_ANY_ERROR_EXCEPT_NACK,
	    RSTDAA,
	    0,
	    NULL,
	    response_cb,
	    &cb_data);

	cb_data.ccc_executed = 0;
	if (usbi3c_submit_commands(usbi3c_dev, USBI3C_DEPENDENT_ON_PREVIOUS) < 0) {
		goto UNREF_AND_EXIT;
	}

	initial_time = time(NULL);
	while (!cb_data.ccc_executed || (current_time > initial_time + TIMEOUT)) {
		/* we submitted the ccc to execute asynchronously, so we need to wait until it executes */
		current_time = time(NULL);
	}

	if (cb_data.response && cb_data.response->attempted == USBI3C_COMMAND_ATTEMPTED && cb_data.response->error_status == USBI3C_SUCCEEDED) {
		ret = 0;
	}

UNREF_AND_EXIT:
	usbi3c_unref_device(usbi3c_dev);
EXIT:
	usbi3c_deinit(&ctx);

	return ret;
}
