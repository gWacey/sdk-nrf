/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <errno.h>

#include "fakes.h"
#include "audio_module/audio_module.h"
#include "common.h"

K_THREAD_STACK_DEFINE(mod_stack, TEST_MOD_THREAD_STACK_SIZE);
K_MEM_SLAB_DEFINE(mod_data_slab, TEST_MOD_DATA_SIZE, FAKE_FIFO_MSG_QUEUE_SIZE, 4);

static struct mod_context mod_context, test_mod_context;
static struct mod_config mod_config = {
	.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 2};
static struct audio_module_functions mod_functions_null = {.open = NULL,
							   .close = NULL,
							   .configuration_set = NULL,
							   .configuration_get = NULL,
							   .start = NULL,
							   .stop = NULL,
							   .data_process = NULL};
static struct audio_module_functions mod_functions_populated = {
	.open = test_open_function,
	.close = test_close_function,
	.configuration_set = test_config_set_function,
	.configuration_get = test_config_get_function,
	.start = test_stop_start_function,
	.stop = test_stop_start_function,
	.data_process = test_data_process_function};
static char *test_base_name = "Test base name";
static struct audio_module_description mod_description = {.name = "Test base name",
							  .type = AUDIO_MODULE_TYPE_IN_OUT,
							  .functions = &mod_functions_null};
static struct audio_module_description test_from_description, test_to_description;
static struct audio_module_parameters mod_parameters = {
	.description = &mod_description,
	.thread = {.stack = mod_stack,
		   .stack_size = TEST_MOD_THREAD_STACK_SIZE,
		   .priority = TEST_MOD_THREAD_PRIORITY,
		   .data_slab = &mod_data_slab,
		   .data_size = TEST_MOD_DATA_SIZE}};
struct audio_module_parameters test_mod_parameters;
static struct audio_module_handle handle;
static struct mod_context *handle_context;
static struct data_fifo mod_fifo_tx, mod_fifo_rx;

/**
 * @brief Set the minimum for a handle.
 *
 * @param handle[in/out]       The handle to the module instance
 * @param state[in]            Pointer to the module's state
 * @param name[in]             Pointer to the module's base name
 * @param description[in/out]  Pointer to the modules description
 */
static void test_handle_set(struct audio_module_handle *hdl, enum audio_module_state state,
			    char *name, struct audio_module_description *description)
{
	hdl->state = state;
	description->name = name;
	hdl->description = description;
}

/**
 * @brief Simple test thread with handle NULL.
 *
 * @param handle[in]  The handle to the module instance
 *
 * @return 0 if successful, error value
 */
static int test_thread_handle(struct audio_module_handle const *const handle)
{
	/* Execute thread */
	while (1) {
		zassert_not_null(handle, NULL, "handle is NULL!");

		k_sleep(K_MSEC(100));
	}

	return 0;
}

/**
 * @brief Simple function to start a test thread with handle.
 *
 * @param handle[in/out]  The handle to the module instance
 *
 * @return 0 if successful, error value
 */
static int start_thread(struct audio_module_handle *handle)
{
	int ret;

	handle->thread_id = k_thread_create(
		&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
		(k_thread_entry_t)test_thread_handle, handle, NULL, NULL,
		K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

	ret = k_thread_name_set(handle->thread_id, handle->name);
	if (ret) {
		return ret;
	}

	k_thread_start(handle->thread_id);

	return 0;
}

/**
 * @brief Function to initialise a handle.
 *
 * @param handle[in/out]     The handle to the module instance
 * @param description[in]    Pointer to the module's description
 * @param context[in/out]    Pointer to the module's context
 * @param configuration[in]  Pointer to the module's configuration
 *
 */
static void test_initialise_handle(struct audio_module_handle *handle,
				   struct audio_module_description const *const description,
				   struct mod_context *context,
				   struct mod_config const *const configuration)
{
	memset(handle, 0, sizeof(struct audio_module_handle));
	memcpy(&handle->name[0], test_instance_name, CONFIG_AUDIO_MODULE_NAME_SIZE);
	handle->description = description;
	handle->state = AUDIO_MODULE_STATE_CONFIGURED;
	handle->dest_count = 0;

	if (configuration != NULL) {
		memcpy(&context->config, configuration, sizeof(struct mod_config));
	}

	if (context != NULL) {
		handle->context = (struct audio_module_context *)context;
	}
}

/**
 * @brief Initialise a list of connections.
 *
 * @param handle_from[in/out]  The handle for the module to initialise the list
 * @param handles_to[in/out]   Pointer to an array of handles to initialise the list with
 * @param list_size[in]        The number of handles to initialise the list with
 * @param use_tx_queue[in]     The state to set for use_tx_queue in the handle
 *
 * @return Number of destinations
 */
static int test_initialise_connection_list(struct audio_module_handle *handle_from,
					   struct audio_module_handle *handles_to, size_t list_size,
					   bool use_tx_queue)
{
	for (int i = 0; i < list_size; i++) {
		sys_slist_append(&handle_from->hdl_dest_list, &handles_to->node);
		handle_from->dest_count += 1;
		handles_to += 1;
	}

	handle_from->use_tx_queue = use_tx_queue;

	if (handle_from->use_tx_queue) {
		handle_from->dest_count += 1;
	}

	return handle_from->dest_count;
}

/**
 * @brief Test a list of connections, both that the handle is in the list and that the list is in
 *        the correct order.
 *
 * @param handle[in]        The handle for the module to test the list
 * @param handles_to[in]    Pointer to an array of handles that should be in the list and are in the
 *                          order expected
 * @param list_size[in]     The number of handles expected in the list
 * @param use_tx_queue[in]  The expected state of use_tx_queue in the handle
 *
 * @return 0 if successful, assert otherwise
 */
static int test_list(struct audio_module_handle *handle, struct audio_module_handle *handles_to,
		     size_t list_size, bool use_tx_queue)
{
	struct audio_module_handle *handle_get;
	int i = 0;

	zassert_equal(handle->dest_count, list_size,
		      "List is the incorrect size, it is %d but should be %d", handle->dest_count,
		      list_size);

	zassert_equal(handle->use_tx_queue, use_tx_queue,
		      "List is the incorrect, use_tx_queue flag is %d but should be %d",
		      handle->use_tx_queue, use_tx_queue);

	SYS_SLIST_FOR_EACH_CONTAINER(&handle->hdl_dest_list, handle_get, node) {
		zassert_equal_ptr(&handles_to[i], handle_get, "List is incorrect for item %d", i);
		i += 1;
	}

	return 0;
}

ZTEST(suite_audio_module_functional, test_number_channels_calculate_fnct)
{
	int ret;
	struct audio_data audio_data;
	uint8_t number_channels;

	audio_data.meta.locations = 0x00000000;
	ret = audio_module_number_channels_calculate(audio_data.meta.locations, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 0,
		      "Calculate number of channels function did not return 0 (%d) channel count",
		      number_channels);

	audio_data.meta.locations = 0x00000001;
	ret = audio_module_number_channels_calculate(audio_data.meta.locations, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 1,
		      "Calculate number of channels function did not return 1 (%d) channel count",
		      number_channels);

	audio_data.meta.locations = 0x80000000;
	ret = audio_module_number_channels_calculate(audio_data.meta.locations, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 1,
		      "Calculate number of channels function did not return 1 (%d) channel count",
		      number_channels);

	audio_data.meta.locations = 0xFFFFFFFF;
	ret = audio_module_number_channels_calculate(audio_data.meta.locations, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 32,
		      "Calculate number of channels function did not return 32 (%d) channel count",
		      number_channels);

	audio_data.meta.locations = 0x55555555;
	ret = audio_module_number_channels_calculate(audio_data.meta.locations, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 16,
		      "Calculate number of channels function did not return 16 (%d) channel count",
		      number_channels);
}

ZTEST(suite_audio_module_functional, test_state_get_fnct)
{
	int ret;
	struct audio_module_handle handle = {0};
	enum audio_module_state state;

	handle.state = AUDIO_MODULE_STATE_CONFIGURED;
	ret = audio_module_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(
		state, AUDIO_MODULE_STATE_CONFIGURED,
		"Get state function did not return AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		AUDIO_MODULE_STATE_CONFIGURED, state);

	handle.state = AUDIO_MODULE_STATE_RUNNING;
	ret = audio_module_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AUDIO_MODULE_STATE_RUNNING,
		      "Get state function did not return AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, state);

	handle.state = AUDIO_MODULE_STATE_STOPPED;
	ret = audio_module_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AUDIO_MODULE_STATE_STOPPED,
		      "Get state function did not return AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, state);
}

ZTEST(suite_audio_module_functional, test_names_get_fnct)
{
	int ret;
	char *base_name;
	char instance_name[CONFIG_AUDIO_MODULE_NAME_SIZE] = {0};
	char *test_base_name_empty = "";
	char *test_base_name_long =
		"Test base name that is longer than the size of CONFIG_AUDIO_MODULE_NAME_SIZE";

	test_handle_set(&handle, AUDIO_MODULE_STATE_CONFIGURED, test_base_name_empty,
			&mod_description);
	memset(&handle.name[0], 0, CONFIG_AUDIO_MODULE_NAME_SIZE);

	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name, "Failed to get the base name: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AUDIO_MODULE_NAME_SIZE,
			  "Failed to get the instance name: %s", instance_name);

	test_handle_set(&handle, AUDIO_MODULE_STATE_CONFIGURED, test_base_name, &mod_description);

	memcpy(&handle.name, "Test instance name", sizeof("Test instance name"));
	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to get the base name in configured state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AUDIO_MODULE_NAME_SIZE,
			  "Failed to get the instance name in configured state: %s", instance_name);

	test_handle_set(&handle, AUDIO_MODULE_STATE_RUNNING, test_base_name, &mod_description);
	memcpy(&handle.name, "Instance name run", sizeof("Instance name run"));

	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to correctly get the base name in running state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AUDIO_MODULE_NAME_SIZE,
			  "Failed to get the instance name in running state: %s", instance_name);

	test_handle_set(&handle, AUDIO_MODULE_STATE_STOPPED, test_base_name, &mod_description);
	memcpy(&handle.name, "Instance name stop", sizeof("Instance name stop"));

	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to correctly get the base name in stopped state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AUDIO_MODULE_NAME_SIZE,
			  "Failed to get the instance name in stopped state: %s", instance_name);

	test_handle_set(&handle, AUDIO_MODULE_STATE_CONFIGURED, test_base_name_long,
			&mod_description);
	memcpy(&handle.name, "Test instance name", sizeof("Test instance name"));

	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to get the base name in configured state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AUDIO_MODULE_NAME_SIZE,
			  "Failed to get the instance name in configured state: %s", instance_name);
}

ZTEST(suite_audio_module_functional, test_reconfigure_fnct)
{
	int ret;

	mod_description.functions = &mod_functions_populated;
	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_audio_module_functional, test_configuration_get_fnct)
{
	int ret;

	mod_description.functions = &mod_functions_populated;
	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_configuration_get(&handle,
					     (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_configuration_get(&handle,
					     (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_configuration_get(&handle,
					     (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Reconfigure state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
		      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_configuration_get(&handle,
					     (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Reconfigure state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_configuration_get(&handle,
					     (struct audio_module_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Reconfigure state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_audio_module_functional, test_stop_null_fnct)
{
	int ret;

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_RUNNING;

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_RUNNING;
	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);
}

ZTEST(suite_audio_module_functional, test_start_null_fnct)
{
	int ret;

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_RUNNING;

	ret = audio_module_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);
}

ZTEST(suite_audio_module_functional, test_stop_fnct)
{
	int ret;

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_RUNNING;

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed stop");

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_RUNNING;

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_STOPPED,
		      "Stop state not AUDIO_MODULE_STATE_STOPPED (%d) rather %d",
		      AUDIO_MODULE_STATE_STOPPED, handle.state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed stop");
}

ZTEST(suite_audio_module_functional, test_start_fnct)
{
	int ret;

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle.state = AUDIO_MODULE_STATE_RUNNING;

	ret = audio_module_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_STOPPED;

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed start");

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AUDIO_MODULE_STATE_CONFIGURED;

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AUDIO_MODULE_STATE_RUNNING,
		      "Start state not AUDIO_MODULE_STATE_RUNNING (%d) rather %d",
		      AUDIO_MODULE_STATE_RUNNING, handle.state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed start");
}

ZTEST(suite_audio_module_functional, test_disconnect_fnct)
{
	int ret;
	int i, j, k;
	int num_destinations;
	char *test_inst_from_name = "TEST instance from";
	char *test_inst_to_name = "TEST instance";
	struct audio_module_handle handle_from;
	struct audio_module_handle handles_to[TEST_CONNECTIONS_NUM];

	for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
		test_initialise_handle(&handles_to[k], &test_to_description, NULL, NULL);
		snprintf(handles_to[k].name, CONFIG_AUDIO_MODULE_NAME_SIZE, "%s %d",
			 test_inst_to_name, k);
	}

	test_from_description.type = AUDIO_MODULE_TYPE_INPUT;
	test_to_description.type = AUDIO_MODULE_TYPE_OUTPUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destinations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_CONNECTIONS_NUM, false);

			for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
				ret = audio_module_disconnect(&handle_from, &handles_to[k], false);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destinations -= 1;

				zassert_equal(handle_from.dest_count, num_destinations,
					      "Destination count should be %d, but is %d",
					      num_destinations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destinations,
					  false);
			}
		}
	}

	test_from_description.type = AUDIO_MODULE_TYPE_INPUT;
	test_to_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destinations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_CONNECTIONS_NUM, false);

			for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
				ret = audio_module_disconnect(&handle_from, &handles_to[k], false);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destinations -= 1;

				zassert_equal(handle_from.dest_count, num_destinations,
					      "Destination count should be %d, but is %d",
					      num_destinations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destinations,
					  false);
			}
		}
	}

	test_from_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	test_to_description.type = AUDIO_MODULE_TYPE_OUTPUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destinations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_CONNECTIONS_NUM, true);

			for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
				ret = audio_module_disconnect(&handle_from, &handles_to[k], false);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destinations -= 1;

				zassert_equal(handle_from.dest_count, num_destinations,
					      "Destination count should be %d, but is %d",
					      num_destinations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destinations, true);
			}
		}

		ret = audio_module_disconnect(&handle_from, NULL, true);
		zassert_equal(ret, 0, "Disconnect function did not return successfully (0): ret %d",
			      ret);

		num_destinations -= 1;

		zassert_equal(handle_from.dest_count, 0, "Destination count is not %d, %d", 0,
			      handle_from.dest_count);
		zassert_equal(handle_from.use_tx_queue, false,
			      "Flag for retuning data from module not false, %d",
			      handle_from.use_tx_queue);
	}

	test_from_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
		memcpy(&handle_from.name, test_inst_from_name, CONFIG_AUDIO_MODULE_NAME_SIZE);
		handle_from.state = i;
		sys_slist_init(&handle_from.hdl_dest_list);
		k_mutex_init(&handle_from.dest_mutex);
		num_destinations = test_initialise_connection_list(&handle_from, NULL, 0, true);

		ret = audio_module_disconnect(&handle_from, NULL, true);

		zassert_equal(ret, 0,
			      "Disconnect function did not return successfully (0): "
			      "ret %d",
			      ret);
		zassert_equal(handle_from.dest_count, 0,
			      "Destination count should be %d, but is %d", 0,
			      handle_from.dest_count);
		zassert_equal(handle_from.use_tx_queue, false,
			      "Flag for retuning data from module not false, %d",
			      handle_from.use_tx_queue);
	}
}

ZTEST(suite_audio_module_functional, test_connect_fnct)
{
	int ret;
	int i, j, k;
	char *test_inst_from_name = "TEST instance from";
	char *test_inst_to_name = "TEST instance";
	struct audio_module_handle handle_from;
	struct audio_module_handle handle_to[TEST_CONNECTIONS_NUM];

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;

	test_from_description.type = AUDIO_MODULE_TYPE_INPUT;
	test_to_description.type = AUDIO_MODULE_TYPE_OUTPUT;

	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;

			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AUDIO_MODULE_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = audio_module_connect(&handle_from, &handle_to[k], false);

				zassert_equal(ret, 0,
					      "Connect function did not return successfully (0): "
					      "ret %d (%d)",
					      ret, k);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			test_list(&handle_from, &handle_to[0], TEST_CONNECTIONS_NUM, false);
		}
	}

	test_from_description.type = AUDIO_MODULE_TYPE_INPUT;
	test_to_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_CONNECTIONS_NUM; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AUDIO_MODULE_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = audio_module_connect(&handle_from, &handle_to[k], false);

				zassert_equal(ret, 0,
					      "Connect function did not return "
					      "successfully (0): ret %d",
					      ret);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			test_list(&handle_from, &handle_to[0], TEST_CONNECTIONS_NUM, false);
		}
	}

	test_from_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	test_to_description.type = AUDIO_MODULE_TYPE_OUTPUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		for (j = AUDIO_MODULE_STATE_CONFIGURED; j <= AUDIO_MODULE_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name,
			       CONFIG_AUDIO_MODULE_NAME_SIZE);
			handle_from.state = i;

			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_CONNECTIONS_NUM - 1; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AUDIO_MODULE_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = audio_module_connect(&handle_from, &handle_to[k], false);

				zassert_equal(ret, 0,
					      "Connect function did not return "
					      "successfully (0): ret %d",
					      ret);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			data_fifo_deinit(&mod_fifo_tx);
			data_fifo_init(&mod_fifo_tx);
			handle_from.thread.msg_tx = &mod_fifo_tx;

			ret = audio_module_connect(&handle_from, NULL, true);
			zassert_equal(ret, 0,
				      "Connect function did not return successfully (0): ret %d",
				      ret);
			zassert_equal(handle_from.dest_count, TEST_CONNECTIONS_NUM,
				      "Destination count is not %d, %d", TEST_CONNECTIONS_NUM,
				      handle_from.dest_count);
		}

		test_list(&handle_from, &handle_to[0], TEST_CONNECTIONS_NUM, true);
	}

	test_from_description.type = AUDIO_MODULE_TYPE_IN_OUT;
	for (i = AUDIO_MODULE_STATE_CONFIGURED; i <= AUDIO_MODULE_STATE_STOPPED; i++) {
		test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
		memcpy(&handle_from.name, test_inst_from_name, CONFIG_AUDIO_MODULE_NAME_SIZE);
		handle_from.state = i;

		data_fifo_deinit(&mod_fifo_tx);
		data_fifo_init(&mod_fifo_tx);
		handle_from.thread.msg_tx = &mod_fifo_tx;

		sys_slist_init(&handle_from.hdl_dest_list);
		k_mutex_init(&handle_from.dest_mutex);

		ret = audio_module_connect(&handle_from, NULL, true);

		zassert_equal(ret, 0, "Connect function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(handle_from.use_tx_queue, true,
			      "Flag for retuning data from module not true, %d",
			      handle_from.use_tx_queue);
		zassert_equal(handle_from.dest_count, 1, "Destination count is not 1, %d",
			      handle_from.dest_count);
	}
}

ZTEST(suite_audio_module_functional, test_close_null_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";
	struct audio_module_functions test_mod_functions = mod_functions_null;
	struct audio_module_description test_mod_description;

	mod_description.functions = &mod_functions_null;

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		/* Register resets */
		DO_FOREACH_FAKE(RESET_FAKE);

		mod_description.type = i;
		test_mod_description = mod_description;
		test_mod_parameters = mod_parameters;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		handle.thread.msg_rx = NULL;
		handle.thread.msg_tx = NULL;

		start_thread(&handle);

		handle.state = AUDIO_MODULE_STATE_CONFIGURED;

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 0,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");

		memset(&handle, 0, sizeof(struct audio_module_handle));
		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		handle.thread.msg_rx = NULL;
		handle.thread.msg_tx = NULL;

		start_thread(&handle);

		handle.state = AUDIO_MODULE_STATE_STOPPED;

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 0,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");
	}
}

ZTEST(suite_audio_module_functional, test_close_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";
	struct audio_module_description test_mod_description;
	struct mod_context test_mod_context;
	struct audio_module_functions test_mod_functions = mod_functions_null;

	mod_description.functions = &mod_functions_null;

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		/* Register resets */
		DO_FOREACH_FAKE(RESET_FAKE);

		test_context_set(&test_mod_context, &mod_config);
		mod_context = test_mod_context;

		mod_description.type = i;
		test_mod_description = mod_description;
		test_mod_parameters = mod_parameters;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = 1;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_rx);
		data_fifo_deinit(&mod_fifo_tx);

		data_fifo_init(&mod_fifo_rx);
		data_fifo_init(&mod_fifo_tx);

		handle.thread.msg_rx = &mod_fifo_rx;
		handle.thread.msg_tx = &mod_fifo_tx;

		start_thread(&handle);

		handle.state = AUDIO_MODULE_STATE_CONFIGURED;

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 2,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");

		memset(&handle, 0, sizeof(struct audio_module_handle));
		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_rx);
		data_fifo_deinit(&mod_fifo_tx);

		data_fifo_init(&mod_fifo_rx);
		data_fifo_init(&mod_fifo_tx);

		handle.thread.msg_rx = &mod_fifo_rx;
		handle.thread.msg_tx = &mod_fifo_tx;

		handle.state = AUDIO_MODULE_STATE_STOPPED;

		start_thread(&handle);

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 4,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		/* Register resets */
		DO_FOREACH_FAKE(RESET_FAKE);

		test_context_set(&test_mod_context, &mod_config);
		mod_context = test_mod_context;

		mod_description.type = i;
		test_mod_description = mod_description;
		test_mod_parameters = mod_parameters;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_rx);

		data_fifo_init(&mod_fifo_rx);

		handle.thread.msg_rx = &mod_fifo_rx;
		handle.thread.msg_tx = NULL;

		start_thread(&handle);

		handle.state = AUDIO_MODULE_STATE_CONFIGURED;

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 1,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");

		memset(&handle, 0, sizeof(struct audio_module_handle));
		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_rx);

		data_fifo_init(&mod_fifo_rx);

		handle.thread.msg_rx = &mod_fifo_rx;
		handle.thread.msg_tx = NULL;

		handle.state = AUDIO_MODULE_STATE_STOPPED;

		start_thread(&handle);

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 2,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		/* Register resets */
		DO_FOREACH_FAKE(RESET_FAKE);

		test_context_set(&test_mod_context, &mod_config);
		mod_context = test_mod_context;

		mod_description.type = i;
		test_mod_description = mod_description;
		test_mod_parameters = mod_parameters;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_tx);

		data_fifo_init(&mod_fifo_tx);

		handle.thread.msg_rx = NULL;
		handle.thread.msg_tx = &mod_fifo_tx;

		start_thread(&handle);

		handle.state = AUDIO_MODULE_STATE_CONFIGURED;

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 1,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");

		memset(&handle, 0, sizeof(struct audio_module_handle));
		memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
		handle.description = &mod_description;
		handle.use_tx_queue = true;
		handle.dest_count = TEST_MODULES_NUM;
		handle.thread.stack = mod_stack;
		handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
		handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
		handle.thread.data_slab = &mod_data_slab;
		handle.thread.data_size = TEST_MOD_DATA_SIZE;
		handle.context = (struct audio_module_context *)&mod_context;

		/* Fake internal empty data FIFO success */
		data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
		data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

		data_fifo_deinit(&mod_fifo_tx);

		data_fifo_init(&mod_fifo_tx);

		handle.thread.msg_rx = NULL;
		handle.thread.msg_tx = &mod_fifo_tx;

		handle.state = AUDIO_MODULE_STATE_STOPPED;

		start_thread(&handle);

		ret = audio_module_close(&handle);

		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(data_fifo_empty_fake.call_count, 2,
			      "Failed close, data FIFO empty called %d times",
			      data_fifo_empty_fake.call_count);
		zassert_mem_equal(&mod_description, &test_mod_description,
				  sizeof(struct audio_module_description),
				  "Failed close, modified the modules description");
		zassert_mem_equal(&mod_functions_null, &test_mod_functions,
				  sizeof(struct audio_module_functions),
				  "Failed close, modified the modules functions");
		zassert_mem_equal(&mod_parameters, &test_mod_parameters,
				  sizeof(struct audio_module_parameters),
				  "Failed close, modified the modules parameter settings");
		zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
				  "Failed close, modified the modules context");
	}
}

ZTEST(suite_audio_module_functional, test_open_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";

	mod_description.functions = &mod_functions_populated;

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;

	data_fifo_deinit(&mod_fifo_rx);
	data_fifo_deinit(&mod_fifo_tx);

	data_fifo_init(&mod_fifo_rx);
	data_fifo_init(&mod_fifo_tx);

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		test_context_set(&test_mod_context, &mod_config);

		mod_parameters.thread.msg_rx = &mod_fifo_rx;
		mod_parameters.thread.msg_tx = &mod_fifo_tx;

		mod_description.type = i;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		handle.state = AUDIO_MODULE_STATE_UNDEFINED;

		ret = audio_module_open(
			&mod_parameters, (struct audio_module_configuration *)&mod_config,
			test_inst_name, (struct audio_module_context *)&mod_context, &handle);

		zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
		zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
			      "Open state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
			      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
		zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
				  "Failed open, name should be %s, but is %s", test_inst_name,
				  handle.name);
		zassert_mem_equal(handle.description, &mod_description,
				  sizeof(struct audio_module_description),
				  "Failed open, module descriptions differ");
		zassert_mem_equal(handle.description->functions, &mod_functions_populated,
				  sizeof(struct audio_module_functions),
				  "Failed open, module function pointers differ");
		zassert_mem_equal(&handle.thread, &mod_parameters.thread,
				  sizeof(struct audio_module_thread_configuration),
				  "Failed open, module thread settings differ");
		zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
				  "Failed open, module contexts differ");
		zassert_equal(handle.use_tx_queue, false,
			      "Open failed use_tx_queue is not false: %d", handle.use_tx_queue);
		zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
			      handle.dest_count);

		k_thread_abort(handle.thread_id);
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		test_context_set(&test_mod_context, &mod_config);

		mod_parameters.thread.msg_rx = NULL;
		mod_parameters.thread.msg_tx = &mod_fifo_tx;

		mod_description.type = i;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		handle.state = AUDIO_MODULE_STATE_UNDEFINED;

		ret = audio_module_open(
			&mod_parameters, (struct audio_module_configuration *)&mod_config,
			test_inst_name, (struct audio_module_context *)&mod_context, &handle);

		zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
		zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
			      "Open state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
			      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
		zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
				  "Failed open, name should be %s, but is %s", test_inst_name,
				  handle.name);
		zassert_mem_equal(handle.description, &mod_description,
				  sizeof(struct audio_module_description),
				  "Failed open, module descriptions differ");
		zassert_mem_equal(handle.description->functions, &mod_functions_populated,
				  sizeof(struct audio_module_functions),
				  "Failed open, module function pointers differ");
		zassert_mem_equal(&handle.thread, &mod_parameters.thread,
				  sizeof(struct audio_module_thread_configuration),
				  "Failed open, module thread settings differ");
		zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
				  "Failed open, module contexts differ");
		zassert_equal(handle.use_tx_queue, false,
			      "Open failed use_tx_queue is not false: %d", handle.use_tx_queue);
		zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
			      handle.dest_count);

		k_thread_abort(handle.thread_id);
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		test_context_set(&test_mod_context, &mod_config);

		mod_parameters.thread.msg_rx = &mod_fifo_rx;
		mod_parameters.thread.msg_tx = NULL;

		mod_description.type = i;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		handle.state = AUDIO_MODULE_STATE_UNDEFINED;

		ret = audio_module_open(
			&mod_parameters, (struct audio_module_configuration *)&mod_config,
			test_inst_name, (struct audio_module_context *)&mod_context, &handle);

		zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
		zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
			      "Open state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
			      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
		zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
				  "Failed open, name should be %s, but is %s", test_inst_name,
				  handle.name);
		zassert_mem_equal(handle.description, &mod_description,
				  sizeof(struct audio_module_description),
				  "Failed open, module descriptions differ");
		zassert_mem_equal(handle.description->functions, &mod_functions_populated,
				  sizeof(struct audio_module_functions),
				  "Failed open, module function pointers differ");
		zassert_mem_equal(&handle.thread, &mod_parameters.thread,
				  sizeof(struct audio_module_thread_configuration),
				  "Failed open, module thread settings differ");
		zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
				  "Failed open, module contexts differ");
		zassert_equal(handle.use_tx_queue, false,
			      "Open failed use_tx_queue is not false: %d", handle.use_tx_queue);
		zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
			      handle.dest_count);

		k_thread_abort(handle.thread_id);
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		test_context_set(&test_mod_context, &mod_config);

		mod_parameters.thread.msg_rx = &mod_fifo_rx;
		mod_parameters.thread.msg_tx = &mod_fifo_tx;
		mod_parameters.thread.data_slab = NULL;
		mod_parameters.thread.data_size = TEST_MOD_DATA_SIZE;

		mod_description.type = i;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		handle.state = AUDIO_MODULE_STATE_UNDEFINED;

		ret = audio_module_open(
			&mod_parameters, (struct audio_module_configuration *)&mod_config,
			test_inst_name, (struct audio_module_context *)&mod_context, &handle);

		zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
		zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
			      "Open state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
			      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
		zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
				  "Failed open, name should be %s, but is %s", test_inst_name,
				  handle.name);
		zassert_mem_equal(handle.description, &mod_description,
				  sizeof(struct audio_module_description),
				  "Failed open, module descriptions differ");
		zassert_mem_equal(handle.description->functions, &mod_functions_populated,
				  sizeof(struct audio_module_functions),
				  "Failed open, module function pointers differ");
		zassert_mem_equal(&handle.thread, &mod_parameters.thread,
				  sizeof(struct audio_module_thread_configuration),
				  "Failed open, module thread settings differ");
		zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
				  "Failed open, module contexts differ");
		zassert_equal(handle.use_tx_queue, false,
			      "Open failed use_tx_queue is not false: %d", handle.use_tx_queue);
		zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
			      handle.dest_count);

		k_thread_abort(handle.thread_id);
	}

	for (int i = AUDIO_MODULE_TYPE_INPUT; i <= AUDIO_MODULE_TYPE_IN_OUT; i++) {
		test_context_set(&test_mod_context, &mod_config);

		mod_parameters.thread.msg_rx = &mod_fifo_rx;
		mod_parameters.thread.msg_tx = &mod_fifo_tx;
		mod_parameters.thread.data_slab = &mod_data_slab;
		mod_parameters.thread.data_size = 0;

		mod_description.type = i;

		memset(&handle, 0, sizeof(struct audio_module_handle));

		handle.state = AUDIO_MODULE_STATE_UNDEFINED;

		ret = audio_module_open(
			&mod_parameters, (struct audio_module_configuration *)&mod_config,
			test_inst_name, (struct audio_module_context *)&mod_context, &handle);

		zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
		zassert_equal(handle.state, AUDIO_MODULE_STATE_CONFIGURED,
			      "Open state not AUDIO_MODULE_STATE_CONFIGURED (%d) rather %d",
			      AUDIO_MODULE_STATE_CONFIGURED, handle.state);
		zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
				  "Failed open, name should be %s, but is %s", test_inst_name,
				  handle.name);
		zassert_mem_equal(handle.description, &mod_description,
				  sizeof(struct audio_module_description),
				  "Failed open, module descriptions differ");
		zassert_mem_equal(handle.description->functions, &mod_functions_populated,
				  sizeof(struct audio_module_functions),
				  "Failed open, module function pointers differ");
		zassert_mem_equal(&handle.thread, &mod_parameters.thread,
				  sizeof(struct audio_module_thread_configuration),
				  "Failed open, module thread settings differ");
		zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
				  "Failed open, module contexts differ");
		zassert_equal(handle.use_tx_queue, false,
			      "Open failed use_tx_queue is not false: %d", handle.use_tx_queue);
		zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
			      handle.dest_count);

		k_thread_abort(handle.thread_id);
	}
}

ZTEST(suite_audio_module_functional, test_data_tx_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";
	size_t size;
	char test_data[TEST_MOD_DATA_SIZE];
	struct audio_data audio_data = {0};
	struct audio_module_message *msg_rx;

	test_context_set(&mod_context, &mod_config);

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_pointer_first_vacant_get_fake.custom_fake =
		fake_data_fifo_pointer_first_vacant_get__succeeds;
	data_fifo_block_lock_fake.custom_fake = fake_data_fifo_block_lock__succeeds;
	data_fifo_pointer_last_filled_get_fake.custom_fake =
		fake_data_fifo_pointer_last_filled_get__succeeds;
	data_fifo_block_free_fake.custom_fake = fake_data_fifo_block_free__succeeds;

	data_fifo_deinit(&mod_fifo_rx);

	data_fifo_init(&mod_fifo_rx);

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.thread.msg_rx = &mod_fifo_rx;
	handle.thread.msg_tx = NULL;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.state = AUDIO_MODULE_STATE_RUNNING;
	handle.context = (struct audio_module_context *)&mod_context;

	for (int i = 0; i < TEST_MOD_DATA_SIZE; i++) {
		test_data[i] = TEST_MOD_DATA_SIZE - i;
	}

	audio_data.data = &test_data[0];
	audio_data.data_size = TEST_MOD_DATA_SIZE;

	ret = audio_module_data_tx(&handle, &audio_data, NULL);
	zassert_equal(ret, 0, "Data TX function did not return successfully (0):: ret %d", ret);

	ret = data_fifo_pointer_last_filled_get(&mod_fifo_rx, (void **)&msg_rx, &size, K_NO_WAIT);
	zassert_equal(ret, 0, "Data TX function did not return 0: ret %d", 0, ret);
	zassert_mem_equal(msg_rx->audio_data.data, &test_data[0], TEST_MOD_DATA_SIZE,
			  "Failed open, module contexts differ");
	zassert_equal(msg_rx->audio_data.data_size, TEST_MOD_DATA_SIZE,
		      "Failed Data TX-RX function, data sizes differs");
	zassert_equal(data_fifo_pointer_first_vacant_get_fake.call_count, 1,
		      "Data TX-RX failed to get item, data FIFO get called %d times",
		      data_fifo_pointer_first_vacant_get_fake.call_count);
	zassert_equal(data_fifo_block_lock_fake.call_count, 1,
		      "Failed to send item, data FIFO send called %d times",
		      data_fifo_pointer_first_vacant_get_fake.call_count);
}

ZTEST(suite_audio_module_functional, test_data_rx_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";
	char test_data[TEST_MOD_DATA_SIZE];
	char data[TEST_MOD_DATA_SIZE] = {0};
	struct audio_data audio_data_in;
	struct audio_data audio_data_out = {0};
	struct audio_module_message *data_msg_tx;

	test_context_set(&mod_context, &mod_config);

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_pointer_first_vacant_get_fake.custom_fake =
		fake_data_fifo_pointer_first_vacant_get__succeeds;
	data_fifo_block_lock_fake.custom_fake = fake_data_fifo_block_lock__succeeds;
	data_fifo_pointer_last_filled_get_fake.custom_fake =
		fake_data_fifo_pointer_last_filled_get__succeeds;
	data_fifo_block_free_fake.custom_fake = fake_data_fifo_block_free__succeeds;

	data_fifo_deinit(&mod_fifo_tx);

	data_fifo_init(&mod_fifo_tx);

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.thread.msg_rx = NULL;
	handle.thread.msg_tx = &mod_fifo_tx;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.state = AUDIO_MODULE_STATE_RUNNING;
	handle.context = (struct audio_module_context *)&mod_context;

	ret = data_fifo_pointer_first_vacant_get(handle.thread.msg_tx, (void **)&data_msg_tx,
						 K_NO_WAIT);
	/* fill data */
	for (int i = 0; i < TEST_MOD_DATA_SIZE; i++) {
		test_data[i] = TEST_MOD_DATA_SIZE - i;
	}

	audio_data_in.data = &test_data[0];
	audio_data_in.data_size = TEST_MOD_DATA_SIZE;

	memcpy(&data_msg_tx->audio_data, &audio_data_in, sizeof(struct audio_data));
	data_msg_tx->tx_handle = NULL;
	data_msg_tx->response_cb = NULL;

	ret = data_fifo_block_lock(handle.thread.msg_tx, (void **)&data_msg_tx,
				   sizeof(struct audio_module_message));

	audio_data_out.data = &data[0];
	audio_data_out.data_size = TEST_MOD_DATA_SIZE;

	ret = audio_module_data_rx(&handle, &audio_data_out, K_NO_WAIT);
	zassert_equal(ret, 0, "Data RX function did not return successfully (0):: ret %d", ret);
	zassert_mem_equal(&test_data[0], audio_data_out.data, TEST_MOD_DATA_SIZE,
			  "Failed Data RX function, data differs");
	zassert_equal(audio_data_in.data_size, audio_data_out.data_size,
		      "Failed Data TX-RX function, data sizes differs");
	zassert_equal(data_fifo_pointer_last_filled_get_fake.call_count, 1,
		      "Data TX-RX function failed to get item, data FIFO get called %d times",
		      data_fifo_pointer_last_filled_get_fake.call_count);
	zassert_equal(data_fifo_block_free_fake.call_count, 1,
		      "Data TX-RX function failed to free item, data FIFO free called %d times",
		      data_fifo_block_free_fake.call_count);
}

ZTEST(suite_audio_module_functional, test_data_tx_rx_fnct)
{
	int ret;
	char *test_inst_name = "TEST instance 1";
	char test_data[TEST_MOD_DATA_SIZE];
	char data[TEST_MOD_DATA_SIZE] = {0};
	struct audio_data audio_data_in;
	struct audio_data audio_data_out;

	test_context_set(&mod_context, &mod_config);

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_pointer_first_vacant_get_fake.custom_fake =
		fake_data_fifo_pointer_first_vacant_get__succeeds;
	data_fifo_block_lock_fake.custom_fake = fake_data_fifo_block_lock__succeeds;
	data_fifo_pointer_last_filled_get_fake.custom_fake =
		fake_data_fifo_pointer_last_filled_get__succeeds;
	data_fifo_block_free_fake.custom_fake = fake_data_fifo_block_free__succeeds;

	data_fifo_deinit(&mod_fifo_rx);
	data_fifo_deinit(&mod_fifo_tx);

	data_fifo_init(&mod_fifo_rx);
	data_fifo_init(&mod_fifo_tx);

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.thread.stack = mod_stack;
	handle.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
	handle.thread.priority = TEST_MOD_THREAD_PRIORITY;
	handle.thread.msg_rx = &mod_fifo_rx;
	handle.thread.msg_tx = &mod_fifo_tx;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.state = AUDIO_MODULE_STATE_RUNNING;
	handle.context = (struct audio_module_context *)&mod_context;

	for (int i = 0; i < TEST_MOD_DATA_SIZE; i++) {
		test_data[i] = TEST_MOD_DATA_SIZE - i;
	}

	audio_data_in.data = &test_data[0];
	audio_data_in.data_size = TEST_MOD_DATA_SIZE;
	audio_data_out.data = &data[0];
	audio_data_out.data_size = TEST_MOD_DATA_SIZE;

	ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_in, &audio_data_out, K_NO_WAIT);
	zassert_equal(ret, 0, "Data TX-RX function did not return successfully (0):: ret %d", ret);
	zassert_mem_equal(audio_data_in.data, audio_data_out.data, TEST_MOD_DATA_SIZE,
			  "Failed Data TX-RX function, data returned differs");
	zassert_equal(audio_data_in.data_size, audio_data_out.data_size,
		      "Failed Data TX-RX function, data sizes differs");
	zassert_equal(data_fifo_pointer_first_vacant_get_fake.call_count, 1,
		      "Data TX-RX failed to get item, data FIFO get called %d times",
		      data_fifo_pointer_first_vacant_get_fake.call_count);
	zassert_equal(data_fifo_block_lock_fake.call_count, 1,
		      "Failed to send item, data FIFO send called %d times",
		      data_fifo_pointer_first_vacant_get_fake.call_count);
	zassert_equal(data_fifo_pointer_last_filled_get_fake.call_count, 1,
		      "Data TX-RX function failed to get item, data FIFO get called %d times",
		      data_fifo_pointer_last_filled_get_fake.call_count);
	zassert_equal(data_fifo_block_free_fake.call_count, 1,
		      "Data TX-RX function failed to free item, data FIFO free called %d times",
		      data_fifo_block_free_fake.call_count);
}
