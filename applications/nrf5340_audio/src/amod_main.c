/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ctype.h>
#include <nrfx_clock.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include "aobj_api.h"
#include "data_fifo.h"
#include "module.h"
#include "amod_api.h"

#define MODULE_INSTANCE_NUM (2)
#define MODULE_DATA_SIZE_BYTES (150)

#define MODULE_DATA_SIZE (4096)
#define MODULE_DATA_SIZE AMOD_DATA_BUF_SET_SIZE(MODULE_DATA_SIZE_BYTES, 2)
#define MODULE_MESSAGE_IN_NUM (2)
#define MODULE_MESSAGE_OUT_NUM (2)
#define MODULE_DATA_NUM (4)

#define MODULE_THREAD_STACK_SIZE (4096)
#define MODULE_THREAD_PRIORITY (2)

K_THREAD_STACK_DEFINE(dummy_1_thread_stack, MODULE_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(dummy_2_thread_stack, MODULE_THREAD_STACK_SIZE);

struct dummy_configuration module_inst_1_config = { .rate = 10,
						    .depth = 100000,
						    .some_text = "testing 1 testing 1" };

struct dummy_configuration module_inst_2_config = { .rate = 20,
						    .depth = 200000,
						    .some_text = "testing 2 testing 2" };

/**
 * @brief Dynamic allocation of memory for a module.
 *
 */
static int mem_alloc_dynamic(struct amod_parameters *parameters,
			     struct amod_configuration mod_config, struct amod_handle *handle,
			     char **in_msg_mem, char **out_msg_mem, char **data_mem)
{
	int ret;
	int handle_size, in_msg_size, out_msg_size, data_size;
	struct _amod_parameters *params = (struct _amod_parameters *)parameters;

	handle_size = amod_query_resource(params, mod_config);
	if (handle_size < 0) {
		LOG_ERR("Querry resource FAILED , ret %d", handle_size);
		return -1;
	}

	*handle = k_alloc(handle_size, K_NO_WAIT);

	if (params->thread.in_msg_num > 0) {
		*in_msg_mem = k_calloc(params->thread.in_msg_num, AMOD_IN_MSG_SIZE);
	} else {
		*in_msg_mem = NULL;
	}

	if (params->thread.out_msg_num > 0) {
		*out_msg_mem = k_calloc(params->thread.out_msg_num, AMOD_OUT_MSG_SIZE);
	} else {
		*out_msg_mem = NULL;
	}

	if (params->thread.out_msg_num > 0) {
		*data_mem = k_calloc(params->thread.data_num, params->thread.data_size);
	} else {
		*data_mem = NULL;
	}

	return 0;
}

/**
 * @brief Basic test of a single module
 *
 */
static int basic_test(struct amod_description *dummpy_parameters, char *instance_name,
		      struct dummy_configuration config, char *in_msg, char *out_msg, char *data,
		      struct amod_handle handle)
{
	char *base_name;
	char *inst_name;
	struct dummy_configuration config_retrieved;

	ret = amod_open(dummpy_parameters, instance_name, in_msg, out_msg, data, MODULE_DATA_SIZE,
			MODULE_DATA_NUM, handle);
	if (ret) {
		LOG_ERR("Open FAILED, ret %d", ret);
		return -1;
	}

	ret = amod_names_get(handle, base_name, inst_name);
	if (ret) {
		LOG_ERR("Failed to get names for module, ret %d", ret);
		return -1;
	}

	if (memcmp(base_name, dummpy_parameters->base_name, strlen(dummpy_parameters->base_name))) {
		LOG_ERR("Failed to read the base name correctly (%s) for module %s", base_name,
			hdl->base_name);
		return -1;
	}

	if (memcmp(inst_name, dummpy_parameters->name, strlen(lc3_dec_parameters->name))) {
		LOG_ERR("Failed to read the instance name correctly (%s) for module %s", inst_name,
			hdl->name);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_OPEN) {
		LOG_ERR("Failed to get the correct state for module %s instance %s, ret %d",
			base_name, inst_name, ret);
		return -1;
	}

	ret = amod_configuration_set(handle, config);
	if (ret) {
		LOG_ERR("Set configuration FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_CONFIGURED) {
		LOG_ERR("Failed to get the correct state for module %s instance %s, state %d",
			base_name, instance_name, state);
		return -1;
	}

	ret = amod_configuration_get(handle, &config_retrieved);
	if (ret != -ENOSYS) {
		LOG_ERR("Get configuration FAILED, ret %d", ret);
		return -1;
	}

	if (memcmp(config, &config_retrieved, sizeof(struct dummy_configuration))) {
		LOG_ERR("Failed to read the configuration back correctly (%s) for module %s",
			inst_name, hdl->name);
		return -1;
	}

	ret = amod_start(handle);
	if (ret) {
		LOG_ERR("Start FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_RUNNING) {
		LOG_ERR("Failed to get the correct state for module %s instance %s, ret %d",
			base_name, instance_name, ret);
		return -1;
	}

	ret = amod_pause(handle);
	if (ret) {
		LOG_ERR("Send command FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_PAUSED) {
		LOG_ERR("Failed to get the correct state for module %s instance %s, ret %d",
			base_name, instance_name, ret);
		return -1;
	}

	return 0;
}

/**
 * @brief Function to start a single module to the configured stage.
 *
 */
static int void module_dynamic_test(void)
{
	int ret;
	struct amod_handle handle_1;
	struct amod_handle handle_2;
	struct amod_handle handle_memory;
	struct amod_parameters dummy_1_parameters;
	struct amod_parameters dummy_2_parameters;
	char *in_msg_1, *in_msg_2;
	char *out_msg_1, *out_msg_2;
	char *data_1, *data_2;

	amod_parameters_configure(dummy_1_parameters, dummy_description, dummy_1_thread_stack,
				  MODULE_THREAD_STACK_SIZE, MODULE_THREAD_PRIORITY,
				  MODULE_MESSAGE_IN_NUM, MODULE_MESSAGE_OUT_NUM);
	if (ret) {
		LOG_ERR("Parameter configuration FAILED , ret %d", ret);
		return -1;
	}

	ret = mem_alloc_dynamic(dummy_1_parameters, &module_inst_1_config, &handle_1, &in_msg_1,
				&out_msg_1, &data_1);
	if (ret) {
		LOG_ERR("Failed to allocate memory, ret %d", ret);
		return -1;
	}

	ret = basic_test(dummy_1_parameters, &module_inst_1_config, "Instance 1", in_msg, out_msg,
			 data, handle_1);
	if (ret) {
		LOG_ERR("Failed to basic tests, ret %d", ret);
		return -1;
	}

	amod_parameters_configure(dummy_2_parameters, dummy_description, dummy_2_thread_stack,
				  MODULE_THREAD_STACK_SIZE, MODULE_THREAD_PRIORITY,
				  MODULE_MESSAGE_IN_NUM, MODULE_MESSAGE_OUT_NUM);
	if (ret) {
		LOG_ERR("Parameter configuration FAILED , ret %d", ret);
		return -1;
	}

	ret = mem_alloc_dynamic(dummy_2_parameters, &module_inst_2_config, &handle_2, &in_msg_2,
				&out_msg_2, &data_2);
	if (ret) {
		LOG_ERR("Failed to allocate memory, ret %d", ret);
		return -1;
	}

	ret = basic_test(dummy_2_parameters, &module_inst_2_config, "Instance 2", in_msg, out_msg,
			 data, handle_2);
	if (ret) {
		LOG_ERR("Failed to basic tests, ret %d", ret);
		return -1;
	}

	/* Re-run tests to check they have not interferred with each other */
	ret = basic_test(dummy_1_parameters, &module_inst_1_config, "Instance 1", in_msg, out_msg,
			 data, handle_1);
	if (ret) {
		LOG_ERR("Failed to basic tests, ret %d", ret);
		return -1;
	}

	ret = basic_test(dummy_2_parameters, &module_inst_2_config, "Instance 2", in_msg, out_msg,
			 data, handle_2);
	if (ret) {
		LOG_ERR("Failed to basic tests, ret %d", ret);
		return -1;
	}

	/* Connect the modules together */
	ret = amod_connect(handle_1, handle_2);
	if (ret) {
		LOG_ERR("Failed to connect, ret %d", ret);
		return -1;
	}

	/* Disconnect the modules */
	ret = amod_disconnect(handle_1, handle_2);
	if (ret) {
		LOG_ERR("Failed to disconnect, ret %d", ret);
		return -1;
	}

	handle_1_memory = handle_1;
	handle_2_memory = handle_2;

	ret = amod_close(&handle_1);
	if (ret) {
		LOG_ERR("Close failed for mosule 1, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle_2);
	if (state != AMOD_STATE_PAUSED) {
		LOG_ERR("Close of module 1 has effected module 2", state);
		return -1;
	}

	ret = amod_close(&handle_2);
	if (ret) {
		LOG_ERR("Close failed for modeule 2, ret %d", ret);
		return -1;
	}

	if (handle_1 != NULL || handle_2 != NULL) {
		LOG_ERR("Close failed to NULL handle");
		return -1;
	}

	k_free(in_msg_1);
	k_free(out_msg_1);
	k_free(data_1);
	k_free(handle_1);
	k_free(in_msg_2);
	k_free(out_msg_2);
	k_free(data_2);
	k_free(handle_2);

	return 0;
}

void main(void)
{
	int ret;

	ret = module_dynamic_test();
	if (ret) {
		LOG_ERR("Failed test");
	}

	LOG_EINF("Passed test");
}
