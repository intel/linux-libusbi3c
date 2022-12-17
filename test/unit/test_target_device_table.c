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

#include "helpers.h"
#include "mocks.h"
#include "target_device_table_i.h"
#include "usb_i.h"
#include <pthread.h>

#define INTERRUPT_REQUEST_ENABLE 0b001
#define CONTROLLER_REQUEST_ENABLE 0b010
#define IBI_TIMESTAMP_ENABLE 0b100

const int I3C_DEVICE_ADDRESS = 0x32;
const int I3C_DEVICE_CONFIG = 0b00000000;
const int I3C_DEVICE_CAPABILITY = 0b000000000111;
const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
const int TARGET_DCR = 10;
const int TARGET_BCR = 20;
const int TARGET_IBI_PRIORITIZATION = INITIAL_IBI_PRIORITIZATION_POOL;

struct test_deps {
	struct target_device_table *table;
	struct usb_context *usb_ctx;
	struct usb_device *usb_dev;
};

struct libusb_device {
	int fake_device_member;
};

int setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)calloc(1, sizeof(struct test_deps));
	mock_usb_init(NULL, RETURN_SUCCESS);
	int ret = usb_context_init(&deps->usb_ctx);
	assert_int_equal(ret, RETURN_SUCCESS);
	struct usb_device **usb_devices = NULL;

	struct libusb_device libusb_device = { .fake_device_member = 1 };
	struct libusb_device *libusb_devices[2];
	libusb_devices[0] = &libusb_device;
	libusb_devices[1] = NULL;

	mock_libusb_get_device_list(libusb_devices, 1);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&libusb_device);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	ret = usb_find_devices(deps->usb_ctx, NULL, &usb_devices);
	assert_int_equal(ret, 1);

	deps->usb_dev = usb_device_ref(usb_devices[0]);
	assert_non_null(deps->usb_dev);

	usb_free_devices(usb_devices, ret);

	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	usb_device_init(deps->usb_dev);

	deps->table = table_init(deps->usb_dev);
	*state = deps;
	return 0;
}

int teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	if (deps->usb_dev) {
		if (usb_device_is_initialized(deps->usb_dev)) {
			mock_libusb_release_interface(NULL, 0);
			mock_libusb_close();
		}
		usb_device_deinit(deps->usb_dev);
	}
	if (deps->table) {
		table_destroy(&deps->table);
	}
	usb_context_deinit(deps->usb_ctx);
	free(deps);
	return 0;
}

int setup_no_table(void **state)
{
	struct test_deps *deps = (struct test_deps *)calloc(1, sizeof(struct test_deps));
	mock_usb_init(NULL, RETURN_SUCCESS);
	int ret = usb_context_init(&deps->usb_ctx);
	assert_int_equal(ret, RETURN_SUCCESS);
	struct usb_device **usb_devices = NULL;

	struct libusb_device libusb_device = { .fake_device_member = 1 };
	struct libusb_device *libusb_devices[2];
	libusb_devices[0] = &libusb_device;
	libusb_devices[1] = NULL;

	mock_libusb_get_device_list(libusb_devices, 1);
	mock_libusb_get_device_descriptor(USBI3C_DeviceClass, 2, 100, RETURN_SUCCESS);
	mock_libusb_ref_device(&libusb_device);
	mock_libusb_free_device_list(libusb_devices, TRUE);

	ret = usb_find_devices(deps->usb_ctx, NULL, &usb_devices);
	assert_int_equal(ret, 1);

	deps->usb_dev = usb_device_ref(usb_devices[0]);
	assert_non_null(deps->usb_dev);

	usb_free_devices(usb_devices, ret);

	mock_libusb_open(NULL, RETURN_SUCCESS);
	mock_libusb_kernel_driver_active(NULL, 1);
	mock_libusb_detach_kernel_driver(NULL, RETURN_SUCCESS);
	mock_libusb_claim_interface(NULL, RETURN_SUCCESS);
	ret = usb_device_init(deps->usb_dev);
	assert_int_equal(ret, RETURN_SUCCESS);

	*state = deps;
	return 0;
}

/* create target device table with null parameter */
void test_negative_target_device_table_null(void **state)
{
	struct target_device_table *table = table_init(NULL);
	assert_null(table);
}

/* test target device table creation */
void test_usbi3c_target_device_table_create(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device_table *table = table_init(deps->usb_dev);
	assert_non_null(table);
	table_destroy(&table);
}

/* test target device table destroy don't fail on NULL parameter */
void test_negative_usbi3c_target_device_table_destroy_null(void **state)
{
	table_destroy(NULL);
}

/* test target device table destroy */
void test_usbi3c_target_device_table_destroy(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device_table *table = table_init(deps->usb_dev);
	assert_non_null(table);
	table_destroy(&table);
	assert_null(table);
}

/* test target device table insert device don't don't fail on null parameter */
void test_negative_usbi3c_target_device_table_insert_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));

	int res = table_insert_device(NULL, device);
	assert_int_equal(res, RETURN_FAILURE);

	res = table_insert_device(deps->table, NULL);
	assert_int_equal(res, RETURN_FAILURE);
	free(device);
}

/* test target device table insert */
void test_usbi3c_target_device_table_insert(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	int res = table_insert_device(deps->table, device);
	assert_int_equal(res, RETURN_SUCCESS);
	assert_non_null(table_get_device(deps->table, 0x01));
}

static void on_insert_callback(uint8_t address, void *user_data)
{
	check_expected(address);
	check_expected(user_data);
}

/* test table enable events with null parameter */
void test_target_device_table_enable_events_null(void **state)
{
	table_enable_events(NULL);
}

/* test target device table add on insert callback with events disabled */
void test_negative_target_device_on_insert_callback_without_events_enabled(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	int user_data = 1;
	table_on_insert_device(deps->table, on_insert_callback, &user_data);
	int res = table_insert_device(deps->table, device);
	assert_int_equal(res, RETURN_SUCCESS);
}

/* test target device table add on_insert_cb callback with null parameter */
void test_negative_target_device_on_insert_callback_null_parameter(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_on_insert_device(NULL, on_insert_callback, NULL);
	table_on_insert_device(deps->table, NULL, NULL);
	table_on_insert_device(NULL, NULL, NULL);
	table_enable_events(deps->table);
	int res = table_insert_device(deps->table, device);
	assert_int_equal(res, RETURN_SUCCESS);
}

/* test target device table insert with callback */
void test_target_device_table_insert_with_callback(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	int user_data = 1;
	expect_value(on_insert_callback, address, 0x01);
	expect_value(on_insert_callback, user_data, &user_data);
	table_enable_events(deps->table);
	table_on_insert_device(deps->table, on_insert_callback, &user_data);
	int res = table_insert_device(deps->table, device);
	assert_int_equal(res, RETURN_SUCCESS);
}

/* test target device table remove don't fail if parameter is null */
void test_negative_usbi3c_target_device_table_remove_null(void **state)
{
	struct target_device *device = table_remove_device(NULL, 0x01);
	assert_null(device);
}

/* test target device table remove don't fail trying to remove not inserted address */
void test_negative_usbi3c_target_device_table_remove_not_inserted(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = table_remove_device(deps->table, 0x01);
	assert_null(device);
}

/* test target device table remove */
void test_usbi3c_target_device_table_remove(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_insert_device(deps->table, device);
	struct target_device *device2 = table_remove_device(deps->table, 0x01);
	assert_null(table_get_device(deps->table, 0x01));
	assert_non_null(device2);
	free(device2);
}

/* test target device table move don't fail if parameter is null */
void test_negative_usbi3c_target_device_table_change_device_address_null(void **state)
{
	int res = table_change_device_address(NULL, 0x01, 0x02);
	assert_int_equal(res, RETURN_FAILURE);
}

/* test target device table move don't try to move the device to an occupied address */
void test_negative_usbi3c_target_device_table_change_device_address_occupied(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_insert_device(deps->table, device);
	int res = table_change_device_address(deps->table, 0x01, 0x01);
	assert_int_equal(res, RETURN_FAILURE);
}

/* test target device table move don't try to move a non existent device */
void test_negative_usbi3c_target_device_table_change_device_address_non_exist(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int res = table_change_device_address(deps->table, 0x01, 0x02);
	assert_int_equal(res, RETURN_FAILURE);
}

/* test target device table move */
void test_usbi3c_target_device_table_change_device_address(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_insert_device(deps->table, device);
	int res = table_change_device_address(deps->table, 0x01, 0x02);
	assert_int_equal(res, RETURN_SUCCESS);
	assert_null(table_get_device(deps->table, 0x01));
	assert_non_null(table_get_device(deps->table, 0x02));
}

/* test target device table address list if parameter table is null */
void test_negative_usbi3c_target_device_table_address_list_table_null(void **state)
{
	uint8_t *addresses = NULL;
	int res = table_address_list(NULL, &addresses);
	assert_int_equal(res, RETURN_FAILURE);

	res = table_address_list(NULL, NULL);
	assert_int_equal(res, RETURN_FAILURE);
}

/* test target device table address list if parameter list is null */
void test_negative_usbi3c_target_device_table_address_list_param_list_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int total = table_address_list(deps->table, NULL);
	assert_int_equal(total, 0);

	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_insert_device(deps->table, device);
	total = table_address_list(deps->table, NULL);
	assert_int_equal(total, 1);
}

/* test target device table address list if table is empty */
void test_negative_usbi3c_target_device_table_address_list_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *addresses = NULL;
	int res = table_address_list(deps->table, &addresses);
	assert_int_equal(res, 0);

	if (addresses) {
		/* in case the test fails we need to free the resource */
		free(addresses);
	}
}

/* test target device table address list */
void test_usbi3c_target_device_table_address_list(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x01;
	table_insert_device(deps->table, device);

	struct target_device *device2 = (struct target_device *)calloc(1, sizeof(struct target_device));
	device2->target_address = 0x02;
	table_insert_device(deps->table, device2);

	uint8_t *addresses = NULL;
	int total = table_address_list(deps->table, &addresses);
	assert_int_equal(total, 2);

	for (int i = 0; i < total; i++) {
		assert_true(addresses[i] == 0x01 || addresses[i] == 0x02);
		assert_int_equal(addresses[i], i + 1);
	}
	free(addresses);
}

/* test target device table get device if parameter is null */
void test_negative_usbi3c_target_device_table_get_device_null(void **state)
{
	struct target_device *device = table_get_device(NULL, 0x01);
	assert_null(device);
}

/* test target device table get device not available device */
void test_negative_usbi3c_target_device_table_get_device_not_available_dev(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = table_get_device(deps->table, 0x01);
	assert_null(device);
}

/* test target device table get device */
void test_usbi3c_target_device_table_get_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = 0x80;
	table_insert_device(deps->table, device);
	struct target_device *device2 = table_get_device(deps->table, 0x80);
	assert_non_null(device2);
	assert_ptr_equal(device2, device);
}

/* test target device table fill from capability buffer don't fail if a parameter is null */
void test_negative_usbi3c_target_device_table_fill_from_capability_buffer_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t buffer = 1;
	int ret = 0;
	ret = table_fill_from_capability_buffer(NULL, &buffer, 1);
	assert_int_equal(ret, RETURN_FAILURE);
	ret = table_fill_from_capability_buffer(deps->table, NULL, 1);
	assert_int_equal(ret, RETURN_FAILURE);
	ret = table_fill_from_capability_buffer(NULL, NULL, 1);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test target device table fill from capability buffer buffer empty */
void test_negative_usbi3c_target_device_table_fill_from_capability_buffer_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t buffer = 0;
	int ret = table_fill_from_capability_buffer(deps->table, &buffer, 0);
	assert_int_equal(ret, RETURN_FAILURE);
}

/* test target device table fill from capability buffer adding new device from buffer */
void test_usbi3c_target_device_table_fill_from_capability_buffer_new_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *cap_buffer = NULL;
	int ret = 0;
	const int DEVICES_IN_BUS = 1;

	// create capability buffer
	cap_buffer = create_i3c_capability_buffer(DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
						  I3C_DEVICE_ADDRESS, I3C_DEVICE_CAPABILITY, DEVICES_IN_BUS);

	uint16_t size = GET_CAPABILITY_HEADER(cap_buffer)->total_length;
	ret = table_fill_from_capability_buffer(deps->table, cap_buffer, size);
	assert_int_equal(ret, RETURN_SUCCESS);
	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	free(cap_buffer);
}

/* test target device table fill from capability buffer updating device from buffer */
void test_usbi3c_target_device_table_fill_from_capability_buffer_update_device(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	unsigned char *cap_buffer = NULL;
	int ret = 0;
	const int DEVICES_IN_BUS = 1;

	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->target_address = TARGET_ADDRESS_1;
	table_insert_device(deps->table, device);

	// create capability buffer
	cap_buffer = create_i3c_capability_buffer(DEVICE_CONTAINS_CAPABILITY_DATA, USBI3C_PRIMARY_CONTROLLER_ROLE, STATIC_DATA,
						  I3C_DEVICE_ADDRESS, I3C_DEVICE_CAPABILITY, DEVICES_IN_BUS);

	uint16_t size = GET_CAPABILITY_HEADER(cap_buffer)->total_length;
	ret = table_fill_from_capability_buffer(deps->table, cap_buffer, size);
	assert_int_equal(ret, RETURN_SUCCESS);

	struct target_device *in_table = table_get_device(deps->table, TARGET_ADDRESS_1);
	assert_non_null(in_table);
	assert_int_equal(in_table->device_capability.disco_major_ver, MIPI_MAJOR_VER);
	assert_int_equal(in_table->device_capability.disco_minor_ver, MIPI_MINOR_VER);
	assert_int_equal(in_table->device_capability.ibi_prioritization, TARGET_IBI_PRIORITIZATION);
	free(cap_buffer);
}

/* test target device table create device table buffer don't fail on null parameter */
void test_negative_usbi3c_target_device_table_create_device_table_buffer_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int res = 0;

	res = table_create_device_table_buffer(deps->table, NULL);
	assert_int_equal(res, RETURN_FAILURE);

	res = table_create_device_table_buffer(NULL, &buffer);
	assert_int_equal(res, RETURN_FAILURE);
	assert_null(buffer);

	res = table_create_device_table_buffer(NULL, NULL);
	assert_int_equal(res, RETURN_FAILURE);
}

/* test target device table create device table buffer with no target devices */
void test_negative_usbi3c_target_device_table_create_device_table_buffer_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int res = 0;
	res = table_create_device_table_buffer(deps->table, &buffer);
	/* if there are no target devices (or they are not known) in the bus we should
	 * still create a valid buffer with a NumEntries of 0, so it would basically just
	 * include the header (4 bytes) */
	assert_int_equal(res, 4);
	assert_non_null(buffer);

	free(buffer);
}

/* test target device table create device table buffer */
void test_usbi3c_target_device_table_create_device_table_buffer(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int res = 0;

	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	device->device_data.bus_characteristic_register = 0x32;
	device->device_data.device_characteristic_register = 0x22;
	device->target_address = 0x10;
	table_insert_device(deps->table, device);

	struct target_device *device2 = (struct target_device *)calloc(1, sizeof(struct target_device));
	device2->device_data.bus_characteristic_register = 0x33;
	device2->device_data.device_characteristic_register = 0x23;
	device2->target_address = 0x11;
	table_insert_device(deps->table, device2);

	int expected_size = TARGET_DEVICE_HEADER_SIZE + (TARGET_DEVICE_ENTRY_SIZE * 2);

	res = table_create_device_table_buffer(deps->table, &buffer);
	assert_int_equal(res, expected_size);

	assert_int_equal(GET_TARGET_DEVICE_TABLE_HEADER(buffer)->table_size, expected_size);

	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 0)->address, 0x10);
	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 0)->bcr, 0x32);
	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 0)->dcr, 0x22);

	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 1)->address, 0x11);
	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 1)->bcr, 0x33);
	assert_int_equal(GET_TARGET_DEVICE_TABLE_ENTRY_N(buffer, 1)->dcr, 0x23);
	free(buffer);
}
/* negative test target device table create target device configuration buffer getting null parameters */
void test_negative_table_create_target_device_config_null(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int ret = 0;

	ret = table_create_set_target_config_buffer(deps->table, 0, 0, NULL);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_null(buffer);

	ret = table_create_set_target_config_buffer(NULL, 0, 0, &buffer);
	assert_int_equal(ret, RETURN_FAILURE);
	assert_null(buffer);
}

/* negative test target device table create target device configuration buffer target device table empty */
void test_negative_table_create_target_device_config_empty(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	uint8_t *buffer = NULL;
	int ret = 0;

	ret = table_create_set_target_config_buffer(deps->table, 0, 0, &buffer);
	/* if there are no target devices (or they are not known) in the bus we should
	 * still create a valid buffer with a NumEntries of 0, so it would basically just
	 * include the header (4 bytes) */
	assert_int_equal(ret, 4);
	assert_non_null(buffer);

	free(buffer);
}

/* test target device table create target device configuration buffer from target device table */
void test_table_create_target_device_config(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *dev1 = calloc(1, sizeof(struct target_device));
	struct target_device *dev2 = calloc(2, sizeof(struct target_device));
	dev1->target_address = 0x01;
	dev2->target_address = 0x02;

	table_insert_device(deps->table, dev1);
	table_insert_device(deps->table, dev2);

	uint8_t *buffer = NULL;
	int ret = 0;

	uint8_t config = INTERRUPT_REQUEST_ENABLE | CONTROLLER_REQUEST_ENABLE | IBI_TIMESTAMP_ENABLE;
	ret = table_create_set_target_config_buffer(deps->table, config, 0x04, &buffer);
	assert_true(ret > 0);
	assert_non_null(buffer);

	assert_int_equal(GET_TARGET_DEVICE_CONFIG_HEADER(buffer)->numentries, 2);
	assert_int_equal(GET_TARGET_DEVICE_CONFIG_HEADER(buffer)->config_change_command_type, CHANGE_CONFIG_COMMAND_TYPE);

	assert_int_equal(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 0)->address, 0x01);
	assert_true(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 0)->target_interrupt_request);
	assert_true(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 0)->controller_role_request);
	assert_false(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 0)->ibi_timestamp);
	assert_int_equal(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 0)->max_ibi_payload_size, 0x04);

	assert_int_equal(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 1)->address, 0x02);
	assert_true(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 1)->target_interrupt_request);
	assert_true(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 1)->controller_role_request);
	assert_false(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 1)->ibi_timestamp);
	assert_int_equal(GET_TARGET_DEVICE_CONFIG_ENTRY_N(buffer, 1)->max_ibi_payload_size, 0x04);
	free(buffer);
}

/* negative test change address notification fail change*/
void test_notification_target_device_table_notification_handle_change_address_notification_fail(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = ALL_ADDRESS_CHANGE_SUCCEEDED
	};

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	uint8_t *change_result_buffer = NULL;
	uint16_t change_result_buffer_size = 0;
	device->target_address = TARGET_ADDRESS_1;

	table_insert_device(deps->table, device);
	change_result_buffer_size = helper_create_address_change_result_buffer(&change_result_buffer,
									       TARGET_ADDRESS_1,
									       TARGET_ADDRESS_2,
									       1,
									       RETURN_FAILURE);
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, change_result_buffer, change_result_buffer_size);
	target_device_table_notification_handle(&notification, deps->table);

	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	assert_null(table_get_device(deps->table, TARGET_ADDRESS_2));

	free(change_result_buffer);
}

/* negative test change address notification fail move device*/
void test_notification_target_device_table_notification_handle_change_address_notification_fail_move(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = ALL_ADDRESS_CHANGE_SUCCEEDED
	};

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	uint8_t *change_result_buffer = NULL;
	uint16_t change_result_buffer_size = 0;
	device->target_address = TARGET_ADDRESS_1;

	table_insert_device(deps->table, device);
	change_result_buffer_size = helper_create_address_change_result_buffer(&change_result_buffer,
									       TARGET_ADDRESS_1,
									       TARGET_ADDRESS_1,
									       1,
									       RETURN_SUCCESS);
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, change_result_buffer, change_result_buffer_size);
	target_device_table_notification_handle(&notification, deps->table);

	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	assert_null(table_get_device(deps->table, TARGET_ADDRESS_2));

	free(change_result_buffer);
}

/* test change address notification */
void test_target_device_table_notification_handle_change_address_notification(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = ALL_ADDRESS_CHANGE_SUCCEEDED
	};

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	uint8_t *change_result_buffer = NULL;
	uint16_t change_result_buffer_size = 0;
	device->target_address = TARGET_ADDRESS_1;

	table_insert_device(deps->table, device);
	change_result_buffer_size = helper_create_address_change_result_buffer(&change_result_buffer,
									       TARGET_ADDRESS_1,
									       TARGET_ADDRESS_2,
									       1,
									       RETURN_SUCCESS);
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, change_result_buffer, change_result_buffer_size);
	target_device_table_notification_handle(&notification, deps->table);

	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_2));

	free(change_result_buffer);
}

/* negative test change address notification fail */
void test_negative_target_device_table_notification_handle_hotjoin(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = HOTJOIN_ADDRESS_ASSIGNMENT_FAILED
	};
	target_device_table_notification_handle(&notification, deps->table);
}

/* test change address notification */
void test_target_device_table_notification_handle_hotjoin(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED
	};

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	const int DEVICES_IN_BUS = 2;
	const int TARGET_CAPABILITY = 0b1000000000;
	const int TARGET_CONFIG = 0b00000000;
	device->target_address = TARGET_ADDRESS_1;

	table_insert_device(deps->table, device);
	unsigned char *hotjoin_buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	int hotjoin_buffer_size = sizeof(struct target_device_table_header) + (sizeof(struct target_device_table_entry) * 2);

	// fake input control transfer
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, hotjoin_buffer, hotjoin_buffer_size);
	target_device_table_notification_handle(&notification, deps->table);

	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_2));

	free(hotjoin_buffer);
}

void *thread(void *data)
{
	struct test_deps *deps = (struct test_deps *)data;
	struct notification notification = {
		.type = NOTIFICATION_ADDRESS_CHANGE_STATUS,
		.code = HOTJOIN_ADDRESS_ASSIGNMENT_SUCCEEDED
	};

	target_device_table_notification_handle(&notification, deps->table);

	return NULL;
}

/* test change address notification on thread */
void test_target_device_table_notification_over_thread(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	struct target_device *device = (struct target_device *)calloc(1, sizeof(struct target_device));

	const int TARGET_ADDRESS_1 = INITIAL_TARGET_ADDRESS_POOL;
	const int TARGET_ADDRESS_2 = INITIAL_TARGET_ADDRESS_POOL + 1;
	const int DEVICES_IN_BUS = 2;
	const int TARGET_CAPABILITY = 0b1000000000;
	const int TARGET_CONFIG = 0b00000000;
	device->target_address = TARGET_ADDRESS_1;

	table_insert_device(deps->table, device);
	unsigned char *hotjoin_buffer = create_target_device_table_buffer(DEVICES_IN_BUS, TARGET_CONFIG, TARGET_CAPABILITY);
	int hotjoin_buffer_size = sizeof(struct target_device_table_header) + (sizeof(struct target_device_table_entry) * 2);

	// fake input control transfer
	fake_transfer_add_data(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, hotjoin_buffer, hotjoin_buffer_size);

	pthread_t tid;
	pthread_create(&tid, NULL, &thread, deps);
	mock_libusb_wait_for_events_trigger(USBI3C_CONTROL_TRANSFER_ENDPOINT_INDEX, RETURN_SUCCESS);
	usb_wait_for_next_event(deps->usb_dev);

	pthread_join(tid, NULL);

	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_1));
	assert_non_null(table_get_device(deps->table, TARGET_ADDRESS_2));

	free(hotjoin_buffer);
}

int main(int argc, char *argv[])
{
	struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_negative_target_device_table_null, setup_no_table, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_create, setup_no_table, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_destroy_null, setup_no_table, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_destroy, setup_no_table, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_insert_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_insert, setup, teardown),
		cmocka_unit_test(test_target_device_table_enable_events_null),
		cmocka_unit_test_setup_teardown(test_negative_target_device_on_insert_callback_without_events_enabled, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_target_device_on_insert_callback_null_parameter, setup, teardown),
		cmocka_unit_test_setup_teardown(test_target_device_table_insert_with_callback, setup, teardown),
		cmocka_unit_test(test_negative_usbi3c_target_device_table_remove_null),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_remove_not_inserted, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_remove, setup, teardown),
		cmocka_unit_test(test_negative_usbi3c_target_device_table_change_device_address_null),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_change_device_address_occupied, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_change_device_address_non_exist, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_change_device_address, setup, teardown),
		cmocka_unit_test(test_negative_usbi3c_target_device_table_address_list_table_null),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_address_list_param_list_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_address_list_empty, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_address_list, setup, teardown),
		cmocka_unit_test(test_negative_usbi3c_target_device_table_get_device_null),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_get_device_not_available_dev, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_get_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_fill_from_capability_buffer_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_fill_from_capability_buffer_empty, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_fill_from_capability_buffer_new_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_fill_from_capability_buffer_update_device, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_create_device_table_buffer_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_usbi3c_target_device_table_create_device_table_buffer_empty, setup, teardown),
		cmocka_unit_test_setup_teardown(test_usbi3c_target_device_table_create_device_table_buffer, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_table_create_target_device_config_null, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_table_create_target_device_config_empty, setup, teardown),
		cmocka_unit_test_setup_teardown(test_table_create_target_device_config, setup, teardown),
		cmocka_unit_test_setup_teardown(test_notification_target_device_table_notification_handle_change_address_notification_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_notification_target_device_table_notification_handle_change_address_notification_fail_move, setup, teardown),
		cmocka_unit_test_setup_teardown(test_target_device_table_notification_handle_change_address_notification, setup, teardown),
		cmocka_unit_test_setup_teardown(test_negative_target_device_table_notification_handle_hotjoin, setup, teardown),
		cmocka_unit_test_setup_teardown(test_target_device_table_notification_handle_hotjoin, setup, teardown),
		cmocka_unit_test_setup_teardown(test_target_device_table_notification_over_thread, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
