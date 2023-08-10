/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <errno.h>

#include "fakes.h"
#include "amod_api.h"
#include "common_test.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(4);

#define TEST_NUM_CONNECTIONS 5

static char *test_instance_name = "Test instance";
static char *test_string = "This is a test string";
static uint32_t test_uint32 = 0XDEADBEEF;

K_THREAD_STACK_DEFINE(mod_stack, TEST_MOD_STACK_SIZE);
K_MEM_SLAB_DEFINE(mod_data_slab, TEST_MOD_DATA_SIZE, TEST_MOD_MSG_QUEUE_SIZE, 4);

/**
 * @brief Simple test thread with handle NULL.
 *
 * @param handle[in]  The handle to the module instance
 *
 * @return 0 if successful, error value
 */
static int test_thread_handle(struct amod_handle *handle)
{
	/* Execute thread */
	while (1) {
		zassert_not_null(handle, NULL, "handle is NULL!");

		/* Sleep for 0.001s */
		k_sleep(K_MSEC(1));
	}

	return 0;
}

/**
 * @brief Simple function to start a test thread with handle.
 *
 * @param handle[in]  The handle to the module instance
 *
 * @return 0 if successful, error value
 */
static int start_thread(struct amod_handle *handle)
{
	int ret;

	handle->thread_id = k_thread_create(
		&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
		(k_thread_entry_t)test_thread_handle, handle, NULL, NULL,
		K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

	ret = k_thread_name_set(handle->thread_id, handle->name);
	if (ret) {
		LOG_INF("Failed to start module %s test thread, ret %d", handle->name, ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Function to initialise a handle.
 *
 * @param handle[in/out]     The handle to the module instance
 * @param description[in]    Pointer to the modules description
 * @param context[in]        Pointer to the modules context
 * @param configuration[in]  Pointer to the module's configuration
 *
 * @return 0 if successful, error otherwise
 */
static void test_initialise_handle(struct amod_handle *handle, struct amod_description *description,
				   struct mod_context *context, struct mod_config *configuration)
{
	memset(handle, 0, sizeof(struct amod_handle));
	memcpy(&handle->name[0], test_instance_name, CONFIG_AMOD_NAME_SIZE);
	handle->description = description;
	handle->state = AMOD_STATE_CONFIGURED;
	handle->previous_state = AMOD_STATE_UNDEFINED;
	handle->dest_count = 0;

	if (configuration != NULL) {
		memcpy(&context->config, configuration, sizeof(struct mod_config));
	}

	if (context != NULL) {
		handle->context = (struct amod_context *)context;
	}
}

/**
 * @brief Test function to set a module's context.
 *
 * @param ctx[out]    The module context to set
 * @param config[in]  Pointer to the module's configuration
 *
 * @return 0 if successful, error otherwise
 */
static void test_context_set(struct mod_context *ctx, struct mod_config *config)
{
	memcpy(&ctx->test_string, test_string, sizeof(test_string));
	ctx->test_uint32 = test_uint32;
	memcpy(&ctx->config, config, sizeof(struct mod_config));
}

/**
 * @brief Test function to open a module.
 *
 * @param handle[in/out]     The handle to the module instance
 * @param configuration[in]  Pointer to the module's configuration
 *
 * @return 0 if successful, error otherwise
 */
static int test_open(struct amod_handle_private *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	test_context_set(ctx, config);
	memcpy(&ctx->test_string, test_string, sizeof(test_string));
	ctx->test_uint32 = test_uint32;
	memcpy(&ctx->config, config, sizeof(struct mod_config));

	return 0;
}

/**
 * @brief Test function to close a module.
 *
 * @param handle[in/out]     The handle to the module instance
 *
 * @return 0 if successful, error otherwise
 */
static int test_close(struct amod_handle_private *handle)
{
	ARG_UNUSED(handle);

	return 0;
}

/**
 * @brief Test function to configure a module.
 *
 * @param handle[in/out]     The handle to the module instance
 * @param configuration[in]  Pointer to the module's configuration to set
 *
 * @return 0 if successful, error otherwise
 */
static int test_config_set(struct amod_handle_private *handle,
			   struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	memcpy(&ctx->config, config, sizeof(struct mod_config));

	return 0;
}

/**
 * @brief Test function to get the configuration of a module.
 *
 * @param handle[in]          The handle to the module instance
 * @param configuration[out]  Pointer to the module's current configuration
 *
 * @return 0 if successful, error otherwise
 */
static int test_config_get(struct amod_handle_private *handle,
			   struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	memcpy(config, &ctx->config, sizeof(struct mod_config));

	return 0;
}

/**
 * @brief Test stop/start function of a module.
 *
 * @param handle[in/out]  The handle for the module to be stopped or started
 *
 * @return 0 if successful, error otherwise
 */
static int test_stop_start(struct amod_handle_private *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;

	ctx->test_string = test_string;
	ctx->test_uint32 = test_uint32;

	return 0;
}

/**
 * @brief Test process data function of a module.
 *
 * @param handle[in/out]  The handle to the module instance
 * @param block_rx[in]    Pointer to the input audio block or NULL for an input module
 * @param block_tx[out]   Pointer to the output audio block or NULL for an output module
 *
 * @return 0 if successful, error otherwise
 */
static int test_data_process(struct amod_handle_private *handle, struct ablk_block *block_rx,
			     struct ablk_block *block_tx)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(block_rx);
	ARG_UNUSED(block_tx);

	return 0;
}

/**
 * @brief Initialise a list of connections.
 *
 * @param handle_from[in/out]  The handle for the module to initialise the list
 * @param handles_to[in]       Pointer to an array of handles to initislise the list with
 * @param list_size[in]        The number of handles to initialise the list with
 * @param data_tx[in]          The state to set for data_tx in the handle
 *
 * @return Number of destinations
 */
static int test_initialise_connection_list(struct amod_handle *handle_from,
					   struct amod_handle *handles_to, size_t list_size,
					   bool data_tx)
{
	for (int i = 0; i < list_size; i++) {
		sys_slist_append(&handle_from->hdl_dest_list, &handles_to->node);
		handle_from->dest_count += 1;
		handles_to += 1;
	}

	handle_from->data_tx = data_tx;

	if (handle_from->data_tx) {
		handle_from->dest_count += 1;
	}

	return handle_from->dest_count;
}

/**
 * @brief Test a list of connections, both that the handle is in the list and that the list is in
 *        the correct order.
 *
 * @param handle[in]      The handle for the module to test the list
 * @param handles_to[in]  Pointer to an array of handles that should be in the list and are in the
 *                        order expected
 * @param list_size[in]   The number of handles expected in the list
 * @param data_tx[in]     The expected state of data_tx in the handle
 *
 * @return 0 if successful, assert otherwise
 */
static int test_list(struct amod_handle *handle, struct amod_handle *handles_to, size_t list_size,
		     bool data_tx)
{
	struct amod_handle *handle_get;
	int i = 0;

	zassert_equal(handle->dest_count, list_size,
		      "List is the incorrect size, it is %d but should be %d", handle->dest_count,
		      list_size);

	zassert_equal(handle->data_tx, data_tx,
		      "List is the incorrect, data_tx flag is %d but should be %d", handle->data_tx,
		      data_tx);

	SYS_SLIST_FOR_EACH_CONTAINER(&handle->hdl_dest_list, handle_get, node) {
		zassert_equal_ptr(&handles_to[i], handle_get, "List is incorrect for item %d", i);
		i += 1;
	}

	return 0;
}

ZTEST(suite_a_mod_functional, test_number_channels_calculate)
{
	int ret;
	struct ablk_block block = {0};
	uint8_t number_channels;

	ret = amod_number_channels_calculate(&block, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 0,
		      "Calculate number of channels function did not return 0 (%d) channel count",
		      number_channels);

	block.format.channel_map = 0x00000001;
	ret = amod_number_channels_calculate(&block, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 1,
		      "Calculate number of channels function did not return 1 (%d) channel count",
		      number_channels);

	block.format.channel_map = 0x80000000;
	ret = amod_number_channels_calculate(&block, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 1,
		      "Calculate number of channels function did not return 1 (%d) channel count",
		      number_channels);

	block.format.channel_map = 0xFFFFFFFF;
	ret = amod_number_channels_calculate(&block, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 32,
		      "Calculate number of channels function did not return 32 (%d) channel count",
		      number_channels);

	block.format.channel_map = 0x55555555;
	ret = amod_number_channels_calculate(&block, &number_channels);
	zassert_equal(
		ret, 0,
		"Calculate number of channels function did not return successfully (0): ret %d",
		ret);
	zassert_equal(number_channels, 16,
		      "Calculate number of channels function did not return 16 (%d) channel count",
		      number_channels);
}

ZTEST(suite_a_mod_functional, test_state_get)
{
	int ret;
	struct amod_handle handle = {0};
	enum amod_state state;

	ret = amod_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AMOD_STATE_UNDEFINED,
		      "Get state function did not return AMOD_STATE_UNDEFINED (%d) rather %d",
		      AMOD_STATE_UNDEFINED, state);

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AMOD_STATE_CONFIGURED,
		      "Get state function did not return AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, state);

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AMOD_STATE_RUNNING,
		      "Get state function did not return AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, state);

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_state_get(&handle, &state);
	zassert_equal(ret, 0, "Get state function did not return successfully (0): ret %d", ret);
	zassert_equal(state, AMOD_STATE_STOPPED,
		      "Get state function did not return AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, state);
}

ZTEST(suite_a_mod_functional, test_names_get)
{
	int ret;
	struct amod_description mod_description = {0};
	struct amod_handle handle = {0};
	char *base_name;
	char instance_name[CONFIG_AMOD_NAME_SIZE] = {0};
	char *test_base_name_empty = "";
	char *test_base_name = "Test base name";
	char *test_base_name_long =
		"Test base name that is longer than the size of CONFIG_AMOD_NAME_SIZE";

	handle.state = AMOD_STATE_CONFIGURED;
	mod_description.name = test_base_name_empty;
	handle.description = &mod_description;
	memset(&handle.name[0], 0, CONFIG_AMOD_NAME_SIZE);
	ret = amod_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name, "Failed to get the base name: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AMOD_NAME_SIZE,
			  "Failed to get the instance name: %s", instance_name);

	handle.state = AMOD_STATE_CONFIGURED;
	mod_description.name = test_base_name;
	handle.description = &mod_description;
	memcpy(&handle.name, "Test instance name", sizeof("Test instance name"));
	ret = amod_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to get the base name in configured state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AMOD_NAME_SIZE,
			  "Failed to get the instance name in configured state: %s", instance_name);

	handle.state = AMOD_STATE_RUNNING;
	mod_description.name = test_base_name;
	handle.description = &mod_description;
	memcpy(&handle.name, "Instance name run", sizeof("Instance name run"));
	ret = amod_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to correctly get the base name in running state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AMOD_NAME_SIZE,
			  "Failed to get the instance name in running state: %s", instance_name);

	handle.state = AMOD_STATE_STOPPED;
	mod_description.name = test_base_name;
	handle.description = &mod_description;
	memcpy(&handle.name, "Instance name stop", sizeof("Instance name stop"));
	ret = amod_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to correctly get the base name in stopped state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AMOD_NAME_SIZE,
			  "Failed to get the instance name in stopped state: %s", instance_name);

	handle.state = AMOD_STATE_CONFIGURED;
	mod_description.name = test_base_name_long;
	handle.description = &mod_description;
	memcpy(&handle.name, "Test instance name", sizeof("Test instance name"));
	ret = amod_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_equal_ptr(base_name, handle.description->name,
			  "Failed to get the base name in configured state: %s",
			  handle.description->name);
	zassert_mem_equal(instance_name, handle.name, CONFIG_AMOD_NAME_SIZE,
			  "Failed to get the instance name in configured state: %s", instance_name);
}

ZTEST(suite_a_mod_functional, test_reconfigure_null_fnct)
{
	int ret;
	struct mod_context mod_context = {0};
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_UNDEFINED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_UNDEFINED,
		      "Reconfigure previous state not AMOD_STATE_UNDEFINED (%d) rather %d",
		      AMOD_STATE_UNDEFINED, handle.previous_state);
	zassert_mem_equal(&mod_context.config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_context.config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_context.config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (% d) rather % d ",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Reconfigure previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&mod_context.config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Reconfigure previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&mod_context.config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_a_mod_functional, test_configuration_get_null_fnct)
{
	int ret;
	struct mod_context mod_context = {0};
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_UNDEFINED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_UNDEFINED,
		      "Reconfigure previous state not AMOD_STATE_UNDEFINED (%d) rather %d",
		      AMOD_STATE_UNDEFINED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Reconfigure state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Reconfigure state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_a_mod_functional, test_reconfigure_fnct)
{
	int ret;
	struct mod_context mod_context = {0};
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = test_config_set,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_UNDEFINED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_UNDEFINED,
		      "Reconfigure previous state not AMOD_STATE_UNDEFINED (%d) rather %d",
		      AMOD_STATE_UNDEFINED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Reconfigure previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_reconfigure(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Reconfigure previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_a_mod_functional, test_configuration_get_fnct)
{
	int ret;
	struct mod_context mod_context = {0};
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = test_config_get,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_UNDEFINED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_UNDEFINED,
		      "Reconfigure previous state not AMOD_STATE_UNDEFINED (%d) rather %d",
		      AMOD_STATE_UNDEFINED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED "
		      "(%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Reconfigure state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d ",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Reconfigure state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Reconfigure previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");

	test_initialise_handle(&handle, &mod_description, &mod_context, &mod_config);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_configuration_get(&handle, (struct amod_configuration *)&mod_config);
	zassert_equal(ret, 0, "Reconfigure function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Reconfigure state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Reconfigure previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_config, &handle_context->config, sizeof(struct mod_config),
			  "Failed reconfigure");
}

ZTEST(suite_a_mod_functional, test_stop_null_fnct)
{
	int ret;
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
}

ZTEST(suite_a_mod_functional, test_start_null_fnct)
{
	int ret;
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_STOPPED;
	ret = amod_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Start previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Start previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);

	test_initialise_handle(&handle, &mod_description, NULL, NULL);
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_STOPPED;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Start previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
}

ZTEST(suite_a_mod_functional, test_stop_fnct)
{
	int ret;
	struct mod_context test_mod_context = {
		.test_string = test_string, .test_uint32 = test_uint32, .config = {0}};
	struct mod_context mod_context = {.test_string = NULL, .test_uint32 = 0, .config = {0}};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = NULL,
						 .stop = test_stop_start,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed stop");

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&test_mod_context, handle_context, sizeof(struct mod_context),
			  "Failed stop");

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_CONFIGURED;
	ret = amod_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Stop state not AMOD_STATE_STOPPED (%d) rather %d", AMOD_STATE_STOPPED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_RUNNING,
		      "Stop previous state not AMOD_STATE_RUNNING (%d) rather %d",
		      AMOD_STATE_RUNNING, handle.previous_state);
	zassert_mem_equal(&test_mod_context, handle_context, sizeof(struct mod_context),
			  "Failed stop");
}

ZTEST(suite_a_mod_functional, test_start_fnct)
{
	int ret;
	struct mod_context test_mod_context = {
		.test_string = test_string, .test_uint32 = test_uint32, .config = {0}};
	struct mod_context mod_context = {.test_string = NULL, .test_uint32 = 0, .config = {0}};
	struct amod_functions mod_1_functions = {.open = NULL,
						 .close = NULL,
						 .configuration_set = NULL,
						 .configuration_get = NULL,
						 .start = test_stop_start,
						 .stop = NULL,
						 .data_process = NULL};
	char *test_base_name = "Test base name";
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_handle handle = {0};
	struct mod_context *handle_context;

	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_RUNNING;
	handle.previous_state = AMOD_STATE_STOPPED;
	ret = amod_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Start previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&mod_context, handle_context, sizeof(struct mod_context), "Failed start");

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_STOPPED;
	handle.previous_state = AMOD_STATE_RUNNING;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_STOPPED,
		      "Start previous state not AMOD_STATE_STOPPED (%d) rather %d",
		      AMOD_STATE_STOPPED, handle.previous_state);
	zassert_mem_equal(&test_mod_context, handle_context, sizeof(struct mod_context),
			  "Failed start");

	mod_context.test_string = NULL;
	mod_context.test_uint32 = 0;
	memset(&mod_context.config, 0, sizeof(struct mod_config));
	test_initialise_handle(&handle, &mod_description, &mod_context, NULL);
	handle_context = (struct mod_context *)handle.context;
	handle.state = AMOD_STATE_CONFIGURED;
	handle.previous_state = AMOD_STATE_STOPPED;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Start state not AMOD_STATE_RUNNING (%d) rather %d", AMOD_STATE_RUNNING,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_CONFIGURED,
		      "Start previous state not AMOD_STATE_CONFIGURED (%d) rather %d",
		      AMOD_STATE_CONFIGURED, handle.previous_state);
	zassert_mem_equal(&test_mod_context, handle_context, sizeof(struct mod_context),
			  "Failed start");
}

ZTEST(suite_a_mod_functional, test_disconnect)
{
	int ret;
	int i, j, k;
	int num_destionations;
	char *test_base_name = "Test base name";
	char *test_inst_from_name = "TEST instance from";
	char *test_inst_to_name = "TEST instance";
	struct amod_functions mod_functions = {.open = NULL,
					       .close = NULL,
					       .configuration_set = NULL,
					       .configuration_get = NULL,
					       .start = NULL,
					       .stop = NULL,
					       .data_process = NULL};
	struct amod_description test_from_description = {.name = test_base_name,
							 .functions = &mod_functions};
	struct amod_handle handle_from;
	struct amod_handle handles_to[TEST_NUM_CONNECTIONS];
	struct amod_description test_to_description = {.name = test_base_name,
						       .functions = &mod_functions};

	for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
		test_initialise_handle(&handles_to[k], &test_to_description, NULL, NULL);
		snprintf(handles_to[k].name, CONFIG_AMOD_NAME_SIZE, "%s %d", test_inst_to_name, k);
	}

	test_from_description.type = AMOD_TYPE_INPUT;
	test_to_description.type = AMOD_TYPE_OUTPUT;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destionations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_NUM_CONNECTIONS, false);

			for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
				ret = amod_disconnect(&handle_from, &handles_to[k]);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destionations -= 1;

				zassert_equal(handle_from.dest_count, num_destionations,
					      "Destination count should be %d, but is %d",
					      num_destionations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destionations,
					  false);
			}
		}
	}

	test_from_description.type = AMOD_TYPE_INPUT;
	test_to_description.type = AMOD_TYPE_PROCESS;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destionations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_NUM_CONNECTIONS, false);

			for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
				ret = amod_disconnect(&handle_from, &handles_to[k]);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destionations -= 1;

				zassert_equal(handle_from.dest_count, num_destionations,
					      "Destination count should be %d, but is %d",
					      num_destionations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destionations,
					  false);
			}
		}
	}

	test_from_description.type = AMOD_TYPE_PROCESS;
	test_to_description.type = AMOD_TYPE_OUTPUT;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);
			num_destionations = test_initialise_connection_list(
				&handle_from, &handles_to[0], TEST_NUM_CONNECTIONS, true);

			for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
				ret = amod_disconnect(&handle_from, &handles_to[k]);

				zassert_equal(
					ret, 0,
					"Disconnect function did not return successfully (0): "
					"ret %d (%d)",
					ret, k);

				num_destionations -= 1;

				zassert_equal(handle_from.dest_count, num_destionations,
					      "Destination count should be %d, but is %d",
					      num_destionations, handle_from.dest_count);

				test_list(&handle_from, &handles_to[k + 1], num_destionations,
					  true);
			}
		}

		ret = amod_disconnect(&handle_from, &handle_from);
		zassert_equal(ret, 0, "Disconnect function did not return successfully (0): ret %d",
			      ret);

		num_destionations -= 1;

		zassert_equal(handle_from.dest_count, 0, "Destination count is not %d, %d", 0,
			      handle_from.dest_count);
		zassert_equal(handle_from.data_tx, false,
			      "Flag for retuning data from module not false, %d",
			      handle_from.data_tx);
	}

	test_from_description.type = AMOD_TYPE_PROCESS;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
		memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
		handle_from.state = i;
		sys_slist_init(&handle_from.hdl_dest_list);
		k_mutex_init(&handle_from.dest_mutex);
		num_destionations = test_initialise_connection_list(&handle_from, NULL, 0, true);

		ret = amod_disconnect(&handle_from, &handle_from);

		zassert_equal(ret, 0,
			      "Disconnect function did not return successfully (0): "
			      "ret %d",
			      ret);
		zassert_equal(handle_from.dest_count, 0,
			      "Destination count should be %d, but is %d", 0,
			      handle_from.dest_count);
		zassert_equal(handle_from.data_tx, false,
			      "Flag for retuning data from module not false, %d",
			      handle_from.data_tx);
	}
}

ZTEST(suite_a_mod_functional, test_connect)
{
	int ret;
	int i, j, k;
	char *test_base_name = "Test base name";
	char *test_inst_from_name = "TEST instance from";
	char *test_inst_to_name = "TEST instance";
	struct amod_functions mod_functions = {.open = NULL,
					       .close = NULL,
					       .configuration_set = NULL,
					       .configuration_get = NULL,
					       .start = NULL,
					       .stop = NULL,
					       .data_process = NULL};
	struct amod_description test_from_description = {.name = test_base_name,
							 .functions = &mod_functions};
	struct amod_handle handle_from;
	struct amod_handle handle_to[TEST_NUM_CONNECTIONS];
	struct amod_description test_to_description = {.name = test_base_name,
						       .functions = &mod_functions};

	test_from_description.type = AMOD_TYPE_INPUT;
	test_to_description.type = AMOD_TYPE_OUTPUT;

	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AMOD_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = amod_connect(&handle_from, &handle_to[k]);

				zassert_equal(ret, 0,
					      "Connect function did not return successfully (0): "
					      "ret %d (%d)",
					      ret, k);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			test_list(&handle_from, &handle_to[0], TEST_NUM_CONNECTIONS, false);
		}
	}

	test_from_description.type = AMOD_TYPE_INPUT;
	test_to_description.type = AMOD_TYPE_PROCESS;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_NUM_CONNECTIONS; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AMOD_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = amod_connect(&handle_from, &handle_to[k]);

				zassert_equal(ret, 0,
					      "Connect function did not return "
					      "successfully (0): ret %d",
					      ret);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			test_list(&handle_from, &handle_to[0], TEST_NUM_CONNECTIONS, false);
		}
	}

	test_from_description.type = AMOD_TYPE_PROCESS;
	test_to_description.type = AMOD_TYPE_OUTPUT;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		for (j = AMOD_STATE_CONFIGURED; j <= AMOD_STATE_STOPPED; j++) {
			test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
			memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
			handle_from.state = i;
			sys_slist_init(&handle_from.hdl_dest_list);
			k_mutex_init(&handle_from.dest_mutex);

			for (k = 0; k < TEST_NUM_CONNECTIONS - 1; k++) {
				test_initialise_handle(&handle_to[k], &test_to_description, NULL,
						       NULL);
				snprintf(handle_to[k].name, CONFIG_AMOD_NAME_SIZE, "%s %d",
					 test_inst_to_name, k);
				handle_to[k].state = j;

				ret = amod_connect(&handle_from, &handle_to[k]);

				zassert_equal(ret, 0,
					      "Connect function did not return "
					      "successfully (0): ret %d",
					      ret);
				zassert_equal(handle_from.dest_count, k + 1,
					      "Destination count is not %d, %d", k + 1,
					      handle_from.dest_count);
			}

			ret = amod_connect(&handle_from, &handle_from);
			zassert_equal(ret, 0,
				      "Connect function did not return successfully (0): ret %d",
				      ret);
			zassert_equal(handle_from.dest_count, TEST_NUM_CONNECTIONS,
				      "Destination count is not %d, %d", TEST_NUM_CONNECTIONS,
				      handle_from.dest_count);
		}

		test_list(&handle_from, &handle_to[0], TEST_NUM_CONNECTIONS, true);
	}

	test_from_description.type = AMOD_TYPE_PROCESS;
	for (i = AMOD_STATE_CONFIGURED; i <= AMOD_STATE_STOPPED; i++) {
		test_initialise_handle(&handle_from, &test_from_description, NULL, NULL);
		memcpy(&handle_from.name, test_inst_from_name, CONFIG_AMOD_NAME_SIZE);
		handle_from.state = i;
		sys_slist_init(&handle_from.hdl_dest_list);
		k_mutex_init(&handle_from.dest_mutex);

		ret = amod_connect(&handle_from, &handle_from);

		zassert_equal(ret, 0, "Connect function did not return successfully (0): ret %d",
			      ret);
		zassert_equal(handle_from.data_tx, true,
			      "Flag for retuning data from module not true, %d",
			      handle_from.data_tx);
		zassert_equal(handle_from.dest_count, 1, "Destination count is not 1, %d",
			      handle_from.dest_count);
	}
}

ZTEST(suite_a_mod_functional, test_close_null)
{
	int ret;
	char *test_base_name = "Test base name";
	char *test_inst_name = "TEST instance 1";
	struct amod_functions mod_functions = {.open = NULL,
					       .close = NULL,
					       .configuration_set = NULL,
					       .configuration_get = NULL,
					       .start = NULL,
					       .stop = NULL,
					       .data_process = NULL};
	struct amod_functions test_mod_functions = mod_functions;
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_functions};
	struct amod_description test_mod_description = mod_description;
	struct amod_parameters mod_parameters = {.description = &mod_description,
						 .thread = {.stack = mod_stack,
							    .stack_size = TEST_MOD_STACK_SIZE,
							    .priority = 2,
							    .data_slab = &mod_data_slab,
							    .data_size = TEST_MOD_DATA_SIZE}};
	struct amod_parameters test_mod_parameters = mod_parameters;
	struct mod_context mod_context = {0};
	struct amod_handle handle;
	struct amod_handle handle_cleared;

	memset(&handle_cleared, 0, sizeof(struct amod_handle));

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.data_tx = true;
	handle.dest_count = TEST_NUM_MODULES;
	handle.thread.stack = mod_stack;
	handle.thread.stack_size = TEST_MOD_STACK_SIZE;
	handle.thread.priority = 2;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.context = (struct amod_context *)&mod_context;

	/* Fake internal empty data FIFO success */
	data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

	handle.thread.msg_rx = NULL;
	handle.thread.msg_tx = NULL;

	start_thread(&handle);

	handle.previous_state = AMOD_STATE_RUNNING;
	handle.state = AMOD_STATE_CONFIGURED;

	ret = amod_close(&handle);

	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
	zassert_equal(data_fifo_empty_fake.call_count, 0,
		      "Failed close, data FIFO empty called %d times",
		      data_fifo_empty_fake.call_count);
	zassert_mem_equal(&handle, &handle_cleared, sizeof(struct amod_handle),
			  "Failed close handle contents are none zero");
	zassert_mem_equal(&mod_description, &test_mod_description, sizeof(struct amod_description),
			  "Failed close, modified the modules description");
	zassert_mem_equal(&mod_functions, &test_mod_functions, sizeof(struct amod_functions),
			  "Failed close, modified the modules functions");
	zassert_mem_equal(&mod_parameters, &test_mod_parameters, sizeof(struct amod_parameters),
			  "Failed close, modified the modules parameter settings");
	zassert_mem_equal(&mod_context, &mod_context, sizeof(struct mod_context),
			  "Failed close, modified the modules context");

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.data_tx = true;
	handle.dest_count = TEST_NUM_MODULES;
	handle.thread.stack = mod_stack;
	handle.thread.stack_size = TEST_MOD_STACK_SIZE;
	handle.thread.priority = 2;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.context = (struct amod_context *)&mod_context;

	/* Fake internal empty data FIFO success */
	data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

	handle.thread.msg_rx = NULL;
	handle.thread.msg_tx = NULL;

	start_thread(&handle);

	handle.previous_state = AMOD_STATE_RUNNING;
	handle.state = AMOD_STATE_STOPPED;

	ret = amod_close(&handle);

	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
	zassert_equal(data_fifo_empty_fake.call_count, 0,
		      "Failed close, data FIFO empty called %d times",
		      data_fifo_empty_fake.call_count);
	zassert_mem_equal(&handle, &handle_cleared, sizeof(struct amod_handle),
			  "Failed close handle contents are none zero");
	zassert_mem_equal(&mod_description, &test_mod_description, sizeof(struct amod_description),
			  "Failed close, modified the modules description");
	zassert_mem_equal(&mod_functions, &test_mod_functions, sizeof(struct amod_functions),
			  "Failed close, modified the modules functions");
	zassert_mem_equal(&mod_parameters, &test_mod_parameters, sizeof(struct amod_parameters),
			  "Failed close, modified the modules parameter settings");
	zassert_mem_equal(&mod_context, &mod_context, sizeof(struct mod_context),
			  "Failed close, modified the modules context");
}

ZTEST(suite_a_mod_functional, test_close)
{
	int ret;
	char *test_base_name = "Test base name";
	char *test_inst_name = "TEST instance 1";
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct amod_functions mod_functions = {.open = test_open,
					       .close = test_close,
					       .configuration_set = test_config_set,
					       .configuration_get = test_config_get,
					       .start = test_stop_start,
					       .stop = test_stop_start,
					       .data_process = test_data_process};
	struct amod_functions test_mod_functions = mod_functions;
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_functions};
	struct amod_description test_mod_description = mod_description;
	struct data_fifo mod_fifo_rx;
	struct data_fifo mod_fifo_tx;
	struct amod_parameters mod_parameters = {.description = &mod_description,
						 .thread = {.stack = mod_stack,
							    .stack_size = TEST_MOD_STACK_SIZE,
							    .priority = 2,
							    .data_slab = &mod_data_slab,
							    .data_size = TEST_MOD_DATA_SIZE}};
	struct amod_parameters test_mod_parameters = mod_parameters;
	struct mod_context mod_context;
	struct mod_context test_mod_context;
	struct amod_handle handle;
	struct amod_handle handle_cleared;

	test_context_set(&test_mod_context, &mod_config);
	mod_context = test_mod_context;

	memset(&handle_cleared, 0, sizeof(struct amod_handle));

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.data_tx = true;
	handle.dest_count = TEST_NUM_MODULES;
	handle.thread.stack = mod_stack;
	handle.thread.stack_size = TEST_MOD_STACK_SIZE;
	handle.thread.priority = 2;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.context = (struct amod_context *)&mod_context;

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

	data_fifo_init(&mod_fifo_tx);
	data_fifo_init(&mod_fifo_rx);

	handle.thread.msg_rx = &mod_fifo_rx;
	handle.thread.msg_tx = &mod_fifo_tx;

	start_thread(&handle);

	handle.previous_state = AMOD_STATE_RUNNING;
	handle.state = AMOD_STATE_CONFIGURED;

	ret = amod_close(&handle);

	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
	zassert_equal(data_fifo_empty_fake.call_count, 2,
		      "Failed close, data FIFO empty called %d times",
		      data_fifo_empty_fake.call_count);
	zassert_mem_equal(&handle, &handle_cleared, sizeof(struct amod_handle),
			  "Failed close handle contents are none zero");
	zassert_mem_equal(&mod_description, &test_mod_description, sizeof(struct amod_description),
			  "Failed close, modified the modules description");
	zassert_mem_equal(&mod_functions, &test_mod_functions, sizeof(struct amod_functions),
			  "Failed close, modified the modules functions");
	zassert_mem_equal(&mod_parameters, &test_mod_parameters, sizeof(struct amod_parameters),
			  "Failed close, modified the modules parameter settings");
	zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
			  "Failed close, modified the modules context");

	memcpy(&handle.name, test_inst_name, sizeof(test_inst_name));
	handle.description = &mod_description;
	handle.data_tx = true;
	handle.dest_count = TEST_NUM_MODULES;
	handle.thread.stack = mod_stack;
	handle.thread.stack_size = TEST_MOD_STACK_SIZE;
	handle.thread.priority = 2;
	handle.thread.data_slab = &mod_data_slab;
	handle.thread.data_size = TEST_MOD_DATA_SIZE;
	handle.context = (struct amod_context *)&mod_context;

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_empty_fake.custom_fake = fake_data_fifo_empty__succeeds;

	data_fifo_init(&mod_fifo_tx);
	data_fifo_init(&mod_fifo_rx);

	handle.thread.msg_rx = &mod_fifo_rx;
	handle.thread.msg_tx = &mod_fifo_tx;

	handle.previous_state = AMOD_STATE_RUNNING;
	handle.state = AMOD_STATE_STOPPED;

	start_thread(&handle);

	ret = amod_close(&handle);

	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
	zassert_equal(data_fifo_empty_fake.call_count, 4,
		      "Failed close, data FIFO empty called %d times",
		      data_fifo_empty_fake.call_count);
	zassert_mem_equal(&handle, &handle_cleared, sizeof(struct amod_handle),
			  "Failed close handle contents are none zero");
	zassert_mem_equal(&mod_description, &test_mod_description, sizeof(struct amod_description),
			  "Failed close, modified the modules description");
	zassert_mem_equal(&mod_functions, &test_mod_functions, sizeof(struct amod_functions),
			  "Failed close, modified the modules functions");
	zassert_mem_equal(&mod_parameters, &test_mod_parameters, sizeof(struct amod_parameters),
			  "Failed close, modified the modules parameter settings");
	zassert_mem_equal(&mod_context, &test_mod_context, sizeof(struct mod_context),
			  "Failed close, modified the modules context");
}

ZTEST(suite_a_mod_functional, test_open)
{
	int ret;
	char *test_base_name = "Test base name";
	char *test_inst_name = "TEST instance 1";
	struct mod_config mod_config = {
		.test_int1 = 5, .test_int2 = 4, .test_int3 = 3, .test_int4 = 4};
	struct mod_context test_mod_context;
	struct mod_context mod_context = {0};
	struct amod_functions mod_functions = {.open = test_open,
					       .close = test_close,
					       .configuration_set = test_config_set,
					       .configuration_get = test_config_get,
					       .start = test_stop_start,
					       .stop = test_stop_start,
					       .data_process = test_data_process};
	struct amod_description mod_description = {
		.name = test_base_name, .type = AMOD_TYPE_PROCESS, .functions = &mod_functions};
	struct data_fifo mod_fifo_rx;
	struct data_fifo mod_fifo_tx;
	struct amod_parameters mod_parameters = {.description = &mod_description,
						 .thread = {.stack = mod_stack,
							    .stack_size = TEST_MOD_STACK_SIZE,
							    .priority = 2,
							    .data_slab = &mod_data_slab,
							    .data_size = TEST_MOD_DATA_SIZE}};
	struct amod_handle handle;

	test_context_set(&test_mod_context, &mod_config);

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;

	data_fifo_init(&mod_fifo_tx);
	data_fifo_init(&mod_fifo_rx);

	mod_parameters.thread.msg_rx = &mod_fifo_rx;
	mod_parameters.thread.msg_tx = &mod_fifo_tx;

	memset(&handle, 0, sizeof(struct amod_handle));

	handle.previous_state = AMOD_STATE_RUNNING;
	handle.state = AMOD_STATE_UNDEFINED;

	ret = amod_open(&mod_parameters, (struct amod_configuration *)&mod_config, test_inst_name,
			(struct amod_context *)&mod_context, &handle);

	zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Open state not AMOD_STATE_CONFIGURED (%d) rather %d", AMOD_STATE_CONFIGURED,
		      handle.state);
	zassert_equal(handle.previous_state, AMOD_STATE_UNDEFINED,
		      "Open failed previouse state is not AMOD_STATE_UNDEFINED (%d): %d",
		      AMOD_STATE_UNDEFINED, handle.previous_state);
	zassert_mem_equal(&handle.name, test_inst_name, sizeof(test_inst_name),
			  "Failed open, name should be %s, but is %s", test_inst_name, handle.name);
	zassert_mem_equal(handle.description, &mod_description, sizeof(struct amod_description),
			  "Failed open, module descriptions differ");
	zassert_mem_equal(handle.description->functions, &mod_functions,
			  sizeof(struct amod_functions),
			  "Failed open, module function pointers differ");
	zassert_mem_equal(&handle.thread, &mod_parameters.thread,
			  sizeof(struct amod_thread_configuration),
			  "Failed open, module thread settings differ");
	zassert_mem_equal(handle.context, &test_mod_context, sizeof(struct mod_context),
			  "Failed open, module contexts differ");
	zassert_equal(handle.data_tx, false, "Open failed data_tx is not false: %d",
		      handle.data_tx);
	zassert_equal(handle.dest_count, 0, "Open failed dest_count is not 0: %d",
		      handle.dest_count);
}
