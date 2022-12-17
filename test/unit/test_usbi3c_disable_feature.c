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

struct test_deps {
	struct usbi3c_device *usbi3c_dev;
	struct device_info *device_info;
	struct bulk_requests *regular_requests;
};

static int group_setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)malloc(sizeof(struct test_deps));

	deps->usbi3c_dev = helper_usbi3c_init(NULL);
	helper_initialize_controller(deps->usbi3c_dev, NULL, NULL);
	deps->device_info = deps->usbi3c_dev->device_info;
	deps->regular_requests = deps->usbi3c_dev->request_tracker->regular_requests;

	*state = deps;

	return 0;
}

static int group_teardown(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	helper_usbi3c_deinit(&deps->usbi3c_dev, NULL);
	free(deps);

	return 0;
}

static int setup(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;

	deps->usbi3c_dev->device_info = deps->device_info;
	deps->usbi3c_dev->request_tracker->regular_requests = deps->regular_requests;
	deps->usbi3c_dev->device_info->capabilities.handoff_controller_role = TRUE;
	deps->usbi3c_dev->device_info->capabilities.hot_join_capability = TRUE;
	deps->usbi3c_dev->device_info->capabilities.in_band_interrupt_capability = TRUE;
	/* set these features enabled by default */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = TRUE;
	deps->usbi3c_dev->device_info->device_state.handoff_controller_role_enabled = TRUE;
	deps->usbi3c_dev->device_info->device_state.hot_join_enabled = TRUE;
	deps->usbi3c_dev->device_info->device_state.in_band_interrupt_enabled = TRUE;

	return 0;
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_i3c_bus_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_i3c_bus(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_i3c_bus_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_i3c_bus_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_i3c_bus(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent
	 * to clear the feature */
	mock_clear_feature(I3C_BUS, RETURN_SUCCESS);

	ret = usbi3c_disable_i3c_bus(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_i3c_controller_role_handoff(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an unsupported feature gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_unsupported(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* change the capabilities of the device to unsupported for the relevant capability */
	deps->usbi3c_dev->device_info->capabilities.handoff_controller_role = FALSE;

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an already disabled feature gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_already_disabled(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* change the state of the device so the feature shows as already disabled */
	deps->usbi3c_dev->device_info->device_state.handoff_controller_role_enabled = FALSE;

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a failure disabling the feature gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_handoff_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* mock a request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(I3C_CONTROLLER_ROLE_HANDOFF, RETURN_FAILURE);

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.handoff_controller_role_enabled, TRUE);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_i3c_controller_role_handoff(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent
	 * to clear the feature */
	mock_clear_feature(I3C_CONTROLLER_ROLE_HANDOFF, RETURN_SUCCESS);

	ret = usbi3c_disable_i3c_controller_role_handoff(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.handoff_controller_role_enabled, FALSE);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_regular_ibi_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_regular_ibi(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_regular_ibi_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_regular_ibi_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an unsupported feature gracefully */
static void test_negative_usbi3c_disable_regular_ibi_unsupported(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* change the capabilities of the device to unsupported for the relevant capability */
	deps->usbi3c_dev->device_info->capabilities.in_band_interrupt_capability = FALSE;

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an already disabled feature gracefully */
static void test_negative_usbi3c_disable_regular_ibi_already_disabled(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* change the state of the device so the feature shows as already disabled */
	deps->usbi3c_dev->device_info->device_state.in_band_interrupt_enabled = FALSE;

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a failure disabling the feature gracefully */
static void test_negative_usbi3c_disable_regular_ibi_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* mock a request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(REGULAR_IBI, RETURN_FAILURE);

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.in_band_interrupt_enabled, TRUE);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_regular_ibi(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent
	 * to clear the feature */
	mock_clear_feature(REGULAR_IBI, RETURN_SUCCESS);

	ret = usbi3c_disable_regular_ibi(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.in_band_interrupt_enabled, FALSE);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_hot_join_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_hot_join(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_hot_join_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_hot_join_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an unsupported feature gracefully */
static void test_negative_usbi3c_disable_hot_join_unsupported(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* change the capabilities of the device to unsupported for the relevant capability */
	deps->usbi3c_dev->device_info->capabilities.hot_join_capability = FALSE;

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to disable an already disabled feature gracefully */
static void test_negative_usbi3c_disable_hot_join_already_disabled(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* change the state of the device so the feature shows as already disabled */
	deps->usbi3c_dev->device_info->device_state.hot_join_enabled = FALSE;

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a failure disabling the feature gracefully */
static void test_negative_usbi3c_disable_hot_join_failure(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* mock a request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(HOT_JOIN, RETURN_FAILURE);

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.hot_join_enabled, TRUE);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_hot_join(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(HOT_JOIN, RETURN_SUCCESS);

	ret = usbi3c_disable_hot_join(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
	assert_int_equal(deps->usbi3c_dev->device_info->device_state.hot_join_enabled, FALSE);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_regular_ibi_wake_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_regular_ibi_wake(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_regular_ibi_wake_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_regular_ibi_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_regular_ibi_wake_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_regular_ibi_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_regular_ibi_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(REGULAR_IBI_WAKE, RETURN_SUCCESS);

	ret = usbi3c_disable_regular_ibi_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_hot_join_wake_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_hot_join_wake(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_hot_join_wake_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_hot_join_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_hot_join_wake_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_hot_join_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_hot_join_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock a request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(HOT_JOIN_WAKE, RETURN_SUCCESS);

	ret = usbi3c_disable_hot_join_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_request_wake_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_disable_i3c_controller_role_request_wake(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_request_wake_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_disable_i3c_controller_role_request_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_disable_i3c_controller_role_request_wake_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_disable_i3c_controller_role_request_wake(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_disable_i3c_controller_role_request_wake(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(I3C_CONTROLLER_ROLE_REQUEST_WAKE, RETURN_SUCCESS);

	ret = usbi3c_disable_i3c_controller_role_request_wake(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

/* Negative test to validate the function handles a missing context gracefully */
static void test_negative_usbi3c_exit_hdr_mode_for_recovery_missing_context(void **state)
{
	int ret = 0;

	ret = usbi3c_exit_hdr_mode_for_recovery(NULL);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature
 * in a device with not known capabilities gracefully */
static void test_negative_usbi3c_exit_hdr_mode_for_recovery_unknown_capabilities(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* we need to force the controller to have unknown capabilities */
	deps->usbi3c_dev->device_info = NULL;

	ret = usbi3c_exit_hdr_mode_for_recovery(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate the function handles attempting to clear a feature in a non active controller gracefully */
static void test_negative_usbi3c_exit_hdr_mode_for_recovery_not_active_controller(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the device to show as non-active controller */
	deps->usbi3c_dev->device_info->device_state.active_i3c_controller = FALSE;

	ret = usbi3c_exit_hdr_mode_for_recovery(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Negative test to validate that the function handles an invalid request tracker gracefully */
static void test_negative_usbi3c_exit_hdr_mode_for_recovery_no_request_tracker(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = 0;

	/* force the regular request tracker to be invalid */
	deps->usbi3c_dev->request_tracker->regular_requests = NULL;

	ret = usbi3c_exit_hdr_mode_for_recovery(deps->usbi3c_dev);
	assert_int_equal(ret, -1);
}

/* Test to validate the function disables the feature */
static void test_usbi3c_exit_hdr_mode_for_recovery(void **state)
{
	struct test_deps *deps = (struct test_deps *)*state;
	int ret = -1;

	/* mock an request object that resembles that expected to be sent to clear the feature */
	mock_clear_feature(HDR_MODE_EXIT_RECOVERY, RETURN_SUCCESS);

	ret = usbi3c_exit_hdr_mode_for_recovery(deps->usbi3c_dev);
	assert_int_equal(ret, 0);
}

int main(void)
{

	/* Unit tests for the usbi3c_clear_feature() functions */
	const struct CMUnitTest tests[] = {
		/* I3C_BUS */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_bus_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_bus_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_bus_not_active_controller, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_i3c_bus, setup),
		/* I3C_CONTROLLER_ROLE_HANDOFF */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_not_active_controller, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_unsupported, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_already_disabled, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_handoff_failure, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_i3c_controller_role_handoff, setup),
		/* REGULAR_IBI */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_not_active_controller, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_unsupported, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_already_disabled, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_failure, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_regular_ibi, setup),
		/* HOT_JOIN  */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_not_active_controller, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_unsupported, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_already_disabled, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_failure, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_hot_join, setup),
		/* REGULAR_IBI_WAKE  */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_wake_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_wake_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_regular_ibi_wake_not_active_controller, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_regular_ibi_wake, setup),
		/* HOT_JOIN_WAKE */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_wake_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_wake_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_hot_join_wake_not_active_controller, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_hot_join_wake, setup),
		/* I3C_CONTROLLER_ROLE_REQUEST_WAKE */
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_request_wake_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_request_wake_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_disable_i3c_controller_role_request_wake_not_active_controller, setup),
		cmocka_unit_test_setup(test_usbi3c_disable_i3c_controller_role_request_wake, setup),
		/* HDR_MODE_EXIT_RECOVERY */
		cmocka_unit_test_setup(test_negative_usbi3c_exit_hdr_mode_for_recovery_missing_context, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_exit_hdr_mode_for_recovery_unknown_capabilities, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_exit_hdr_mode_for_recovery_not_active_controller, setup),
		cmocka_unit_test_setup(test_negative_usbi3c_exit_hdr_mode_for_recovery_no_request_tracker, setup),
		cmocka_unit_test_setup(test_usbi3c_exit_hdr_mode_for_recovery, setup),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
