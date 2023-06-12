/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>
#include "amod_api.h"

#define MOD_STACK_SIZE	       (1024)
#define MOD_MSG_QUEUE_SIZE     (4)
#define MOD_MSG_QUEUE_SIZE     (4)
#define MOD_DATA_SIZE	       (40)
#define PCM_DATA_OBJECTS_NUM   (4)
#define CODED_DATA_OBJECTS_NUM (4)

struct mod_context {
	char *test_string;
	uint32_t test_uint32;
};

struct mod_config {
	int test_int1;
	int test_int2;
	int test_int3;
	int test_int4;
};

const struct amod_functions mod_1_functions = {.open = NULL,
					       .close = NULL,
					       .configuration_set = NULL,
					       .configuration_get = NULL,
					       .start = NULL,
					       .stop = NULL,
					       .data_process = NULL};

/* Allocate audio memory */
K_THREAD_STACK_DEFINE(mod_thread_stack, MOD_STACK_SIZE);
DATA_FIFO_DEFINE(mod_fifo_tx, MOD_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(mod_fifo_rx, MOD_MSG_QUEUE_SIZE, sizeof(struct amod_message));
K_MEM_SLAB_DEFINE(mod_data_slab, MOD_DATA_SIZE, PCM_DATA_OBJECTS_NUM, 4);

ZTEST(suite_a_mod, test_a_mod_stop_state)
{
	int ret;
	struct amod_handle handle = {0};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Stop returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Stop returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Stop returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Stop returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_stop_null)
{
	int ret;
	struct amod_handle handle = {0};

	ret = amod_stop(NULL);
	zassert_equal(ret, -EINVAL, "Stop function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_start(NULL);
	zassert_equal(ret, 0, "Stop function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Stop returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_start_state)
{
	int ret;
	struct amod_handle handle = {0};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_start(&handle);
	zassert_equal(ret, -ENOTSUP, "Start function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Start returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Start returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_start(&handle);
	zassert_equal(ret, -ENOTSUP, "Start function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Start returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Start returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_start_null)
{
	int ret;
	struct amod_handle handle = {0};

	ret = amod_start(NULL);
	zassert_equal(ret, -EINVAL, "Start function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_start(NULL);
	zassert_equal(ret, 0, "Start function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Start returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_get_config_state)
{
	int ret;
	struct amod_handle handle = {0};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_configuration_get(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration get function did not return -ENOTSUP (129)");
	handle.state = AMOD_STATE_UNDEFINED;

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_get(&handle, config);
	zassert_equal(ret, 0, "Configuration get function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_configuration_get(&handle, config);
	zassert_equal(ret, 0, "Configuration get function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_get(&handle, config);
	zassert_equal(ret, 0, "Configuration get function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_get_config_null)
{
	int ret;
	struct amod_handle handle = {0};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	ret = amod_configuration_get(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	ret = amod_configuration_get(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	ret = amod_configuration_get(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_get(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_get(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_get(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_set_config_state)
{
	int ret;
	struct amod_handle handle = {0};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_configuration_set(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_configuration_set(&handle, config);
	zassert_equal(ret, 0, "Configuration set function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_configuration_set(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_set(&handle, config);
	zassert_equal(ret, 0, "Configuration set function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED,
		      "Configuration set returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_set_config_null)
{
	int ret;
	struct amod_handle handle = {0};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	ret = amod_configuration_set(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_configuration_set(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_configuration_set(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_configuration_set(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_set(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_set(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_configuration_set(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_close_state)
{
	int ret;
	struct amod_handle handle = {0};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_close(&handle);
	zassert_equal(ret, -ENOTSUP, "Close function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Close returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_close(&handle);
	zassert_equal(ret, 0, " did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Close returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_close(&handle);
	zassert_equal(ret, 0, " did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Close returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_close(&handle);
	zassert_equal(ret, 0, "Close did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Close returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_close_null)
{
	int ret;
	struct amod_handle handle = {0};

	ret = amod_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EALREADY (-22)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Close returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_open_thread)
{
	int ret;
	char *inst_name = "Test Module";
	struct amod_description test_description = {0};
	struct amod_parameters test_params_thread = {.description = &test_description};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	test_params_thread.thread.stack = NULL;
	test_params_thread.thread.stack_size = MOD_STACK_SIZE;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = &mod_fifo_rx;
	test_params_thread.thread.msg_tx = &mod_fifo_tx;
	test_params_thread.thread.data_slab = (struct k_mem_slab *)&mod_data_slab;
	test_params_thread.thread.data_size = MOD_DATA_SIZE;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = 0;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = &mod_fifo_rx;
	test_params_thread.thread.msg_tx = &mod_fifo_tx;
	test_params_thread.thread.data_slab = (struct k_mem_slab *)&mod_data_slab;
	test_params_thread.thread.data_size = MOD_DATA_SIZE;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = MOD_STACK_SIZE;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = NULL;
	test_params_thread.thread.msg_tx = &mod_fifo_tx;
	test_params_thread.thread.data_slab = (struct k_mem_slab *)&mod_data_slab;
	test_params_thread.thread.data_size = MOD_DATA_SIZE;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = MOD_STACK_SIZE;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = &mod_fifo_rx;
	test_params_thread.thread.msg_tx = NULL;
	test_params_thread.thread.data_slab = (struct k_mem_slab *)&mod_data_slab;
	test_params_thread.thread.data_size = MOD_DATA_SIZE;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = MOD_STACK_SIZE;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = &mod_fifo_rx;
	test_params_thread.thread.msg_tx = &mod_fifo_tx;
	test_params_thread.thread.data_slab = NULL;
	test_params_thread.thread.data_size = MOD_DATA_SIZE;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, 0, "Open function did not return successfully (0)");

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = MOD_STACK_SIZE;
	test_params_thread.thread.priority = 4;
	test_params_thread.thread.msg_rx = &mod_fifo_rx;
	test_params_thread.thread.msg_tx = &mod_fifo_tx;
	test_params_thread.thread.data_slab = (struct k_mem_slab *)&mod_data_slab;
	test_params_thread.thread.data_size = 0;

	ret = amod_open(&test_params_thread, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, 0, "Open function did not return successfully (0)");
}

ZTEST(suite_a_mod, test_a_mod_open_description)
{
	int ret;
	char *inst_name = "Test Module";
	struct amod_description test_description = {0};
	struct amod_parameters test_params_desc = {0};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	test_params_desc.description = NULL;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = AMOD_TYPE_UNDEFINED;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = -1;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = 4;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = NULL;
	test_description.type = AMOD_TYPE_IN_OUT;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_desc.description->name = "Module 1";
	test_description.type = AMOD_TYPE_IN_OUT;
	test_params_desc.description->functions = NULL;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_a_mod_open_state)
{
	int ret;
	char *inst_name = "Test Module";
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_IN_OUT, .functions = &mod_1_functions};
	struct amod_parameters test_params = {.description = &test_description};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, 0, "Open function did not return successfully (0)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-103)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-103)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Open returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-103)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state");
}

ZTEST(suite_a_mod, test_a_mod_open_null)
{
	int ret;
	char *inst_name = "Test Module";
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_IN_OUT, .functions = &mod_1_functions};
	struct amod_parameters test_params = {.description = &test_description};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	ret = amod_open(NULL, NULL, NULL, NULL, NULL);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	ret = amod_open(NULL, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	ret = amod_open(&test_params, NULL, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	ret = amod_open(&test_params, config, NULL, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	ret = amod_open(&test_params, config, inst_name, NULL, (struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	ret = amod_open(&test_params, config, inst_name, &handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "The module open functio with NULLn did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_open(NULL, NULL, NULL, NULL, NULL);
	zassert_equal(ret, -EINVAL, "Open function did not return -EALREADY (-22)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state");
}

ZTEST_SUITE(suite_a_mod, NULL, NULL, NULL, NULL, NULL);
