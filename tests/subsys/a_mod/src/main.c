/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>
#include "amod_api.h"

#define MOD_NAME_SIZE	       (20)
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

ZTEST(suite_a_mod, test_block_extract_null)
{
	int ret;
	struct ablk_block block = {0};
	enum ablk_type data_type;
	char data[MOD_DATA_SIZE] = {0};
	size_t data_size;
	struct ablk_pcm_format format = {0};
	uint32_t reference_ts;
	uint32_t data_rx_ts;
	bool bad_frame;

	ret = amod_block_data_extract(NULL, &data_type, &data[0], &data_size, &format,
				      &reference_ts, &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, NULL, &data[0], &data_size, &format, &reference_ts,
				      &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, NULL, &data_size, &format, &reference_ts,
				      &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, &data[0], NULL, &format, &reference_ts,
				      &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, &data[0], &data_size, NULL, &reference_ts,
				      &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, &data[0], &data_size, &format, NULL,
				      &data_rx_ts, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, &data[0], &data_size, &format,
				      &reference_ts, NULL, &bad_frame, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");

	ret = amod_block_data_extract(&block, &data_type, &data[0], &data_size, &format,
				      &reference_ts, &data_rx_ts, NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Block data extract block function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_block_attach_null)
{
	int ret;
	struct ablk_block block = {0};
	char data[MOD_DATA_SIZE] = {0};
	struct ablk_pcm_format format = {0};

	ret = amod_block_data_attach(NULL, ABLK_TYPE_LC3, &data[0], sizeof(data), &format, 0, 0,
				     false, NULL);
	zassert_equal(ret, -EINVAL, "Attach to block function did not return -EINVAL (-22)");

	ret = amod_block_data_attach(&block, ABLK_TYPE_UNDEFINED, &data[0], sizeof(data), &format,
				     0, 0, false, NULL);
	zassert_equal(ret, -ENOTSUP, "Attach to block function did not return -EINVAL (-129)");

	ret = amod_block_data_attach(&block, ABLK_TYPE_LC3, NULL, sizeof(data), &format, 0, 0,
				     false, NULL);
	zassert_equal(ret, -EINVAL, "Attach to block function did not return -EINVAL (-22)");

	ret = amod_block_data_attach(&block, ABLK_TYPE_LC3, &data[0], 0, &format, 0, 0, false,
				     NULL);
	zassert_equal(ret, -EINVAL, "Attach to block function did not return -EINVAL (-22)");

	ret = amod_block_data_attach(&block, ABLK_TYPE_LC3, &data[0], sizeof(data), NULL, 0, 0,
				     false, NULL);
	zassert_equal(ret, -EINVAL, "Attach to block function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_number_channels_calculate_null)
{
	int ret;
	struct ablk_block block = {0};
	uint8_t number_channels;

	ret = amod_number_channels_calculate(NULL, &number_channels);
	zassert_equal(ret, -EINVAL,
		      "Calculate number of channels function did not return -EINVAL (-22)");

	ret = amod_number_channels_calculate(&block, NULL);
	zassert_equal(ret, -EINVAL,
		      "Calculate number of channels function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_state_get_null)
{
	int ret;
	struct amod_handle handle = {0};
	enum amod_state state;

	ret = amod_state_get(NULL, &state);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (-22)");

	ret = amod_state_get(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_names_get_null)
{
	int ret;
	struct amod_handle handle = {0};
	char base_name[MOD_NAME_SIZE];
	char instance_name[MOD_NAME_SIZE];

	ret = amod_names_get(NULL, &base_name[0], &instance_name[0]);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (-22)");

	ret = amod_names_get(&handle, NULL, &instance_name[0]);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (-22)");

	ret = amod_names_get(&handle, &base_name[0], NULL);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_names_get(&handle, &base_name[0], &instance_name[0]);
	zassert_equal(ret, -ENOTSUP, "Get names function did not return successfully (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Get names returns with incorrect state");
}

ZTEST(suite_a_mod, test_stop_bad_state)
{
	int ret;
	struct amod_handle handle = {.name = "TEST stop"};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Stop returns with incorrect state");

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Stop returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -ENOTSUP (-114)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Stop returns with incorrect state");
}

ZTEST(suite_a_mod, test_stop_null)
{
	int ret;
	struct amod_handle handle = {0};

	ret = amod_stop(NULL);
	zassert_equal(ret, -EINVAL, "Stop function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_start(NULL);
	zassert_equal(ret, -EINVAL, "Stop function did not return successfully (-22)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Stop returns with incorrect state");
}

ZTEST(suite_a_mod, test_start_bad_state)
{
	int ret;
	struct amod_handle handle = {.name = "TEST start"};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_start(&handle);
	zassert_equal(ret, -ENOTSUP, "Start function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Start returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -ENOTSUP (-114)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Start returns with incorrect state");
}

ZTEST(suite_a_mod, test_start_null)
{
	int ret;
	struct amod_handle handle = {.name = "TEST start"};

	ret = amod_start(NULL);
	zassert_equal(ret, -EINVAL, "Start function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_start(NULL);
	zassert_equal(ret, -EINVAL, "Start function did not return successfully (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Start returns with incorrect state");
}

ZTEST(suite_a_mod, test_config_get_bad_state)
{
	int ret;
	struct amod_handle handle = {.name = "TEST get config"};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration get function did not return -ENOTSUP (129)");
	handle.state = AMOD_STATE_UNDEFINED;
}

ZTEST(suite_a_mod, test_config_get_null)
{
	int ret;
	struct amod_handle handle = {.name = "TEST get config"};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	ret = amod_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	ret = amod_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	ret = amod_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration get function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state");
}

ZTEST(suite_a_mod, test_disconnect_bad_type)
{
	int ret;
	struct amod_description test_description_1 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct amod_description test_description_2 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_TYPE_UNDEFINED,
					.description = &test_description_1};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_TYPE_UNDEFINED,
					.description = &test_description_2};

	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	test_description_1.type = AMOD_TYPE_PROCESS;
	test_description_1.type = AMOD_TYPE_UNDEFINED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_UNDEFINED;
	handle_rx.state = AMOD_TYPE_PROCESS;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	test_description_1.type = AMOD_TYPE_INPUT;
	test_description_1.type = AMOD_TYPE_INPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_OUTPUT;
	handle_rx.state = AMOD_TYPE_INPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_PROCESS;
	handle_rx.state = AMOD_TYPE_OUTPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
}

ZTEST(suite_a_mod, test_disconnect_bad_state)
{
	int ret;
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_STATE_UNDEFINED,
					.description = &test_description};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_STATE_UNDEFINED,
					.description = &test_description};

	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_STATE_CONFIGURED;
	handle_rx.state = AMOD_STATE_UNDEFINED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_STATE_UNDEFINED;
	handle_rx.state = AMOD_STATE_CONFIGURED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
}

ZTEST(suite_a_mod, test_disconnect_null)
{
	int ret;
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_STATE_CONFIGURED,
					.description = &test_description};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_STATE_CONFIGURED,
					.description = &test_description};

	ret = amod_connect(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_connect(NULL, &handle_rx);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_connect(&handle_tx, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_connect_bad_type)
{
	int ret;
	struct amod_description test_description_1 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct amod_description test_description_2 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_TYPE_UNDEFINED,
					.description = &test_description_1};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_TYPE_UNDEFINED,
					.description = &test_description_2};

	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	test_description_1.type = AMOD_TYPE_PROCESS;
	test_description_1.type = AMOD_TYPE_UNDEFINED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_UNDEFINED;
	handle_rx.state = AMOD_TYPE_PROCESS;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	test_description_1.type = AMOD_TYPE_INPUT;
	test_description_1.type = AMOD_TYPE_INPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_OUTPUT;
	handle_rx.state = AMOD_TYPE_INPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");

	handle_tx.state = AMOD_TYPE_PROCESS;
	handle_rx.state = AMOD_TYPE_OUTPUT;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
}

ZTEST(suite_a_mod, test_connect_bad_state)
{
	int ret;
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_STATE_UNDEFINED,
					.description = &test_description};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_STATE_UNDEFINED,
					.description = &test_description};

	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -EINVAL (-129)");

	handle_tx.state = AMOD_STATE_CONFIGURED;
	handle_rx.state = AMOD_STATE_UNDEFINED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -EINVAL (-129)");

	handle_tx.state = AMOD_STATE_UNDEFINED;
	handle_rx.state = AMOD_STATE_CONFIGURED;
	ret = amod_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -EINVAL (-129)");
}

ZTEST(suite_a_mod, test_connect_null)
{
	int ret;
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct amod_handle handle_tx = {.name = "TEST connect 1",
					.state = AMOD_STATE_CONFIGURED,
					.description = &test_description};
	struct amod_handle handle_rx = {.name = "TEST connect 2",
					.state = AMOD_STATE_CONFIGURED,
					.description = &test_description};

	ret = amod_connect(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_connect(NULL, &handle_rx);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_connect(&handle_tx, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_reconfig_bad_state)
{
	int ret;
	struct amod_handle handle = {.name = "TEST set config"};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP, "Configuration set function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Configuration set returns with incorrect state");
}

ZTEST(suite_a_mod, test_reconfig_null)
{
	int ret;
	struct amod_handle handle = {.name = "TEST set config"};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;

	ret = amod_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	ret = amod_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Configuration set function did not return -EINVAL (-22)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state");
}

ZTEST(suite_a_mod, test_close_bad_state)
{
	int ret;
	char *inst_name = "TEST close";
	struct amod_handle handle = {0};

	memcpy(&handle.name, inst_name, sizeof(inst_name));

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EALREADY (-22)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Close returns with incorrect state");

	handle.state = AMOD_STATE_UNDEFINED;
	ret = amod_close(&handle);
	zassert_equal(ret, -ENOTSUP, "Close function did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Close returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_close(&handle);
	zassert_equal(ret, -ENOTSUP, " did not return -ENOTSUP (-129)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Close returns with incorrect state");
}

ZTEST(suite_a_mod, test_close_null)
{
	int ret;

	ret = amod_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_open_bad_thread)
{
	int ret;
	char *inst_name = "TEST open";
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
}

ZTEST(suite_a_mod, test_open_bad_description)
{
	int ret;
	char *inst_name = "TEST open";
	struct amod_description test_description = {0};
	struct amod_parameters test_params_desc = {0};
	struct mod_config config = {0};
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	test_params_desc.description = NULL;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = AMOD_TYPE_UNDEFINED;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = -1;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = "Module 1";
	test_description.type = 4;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_description.name = NULL;
	test_description.type = AMOD_TYPE_PROCESS;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");

	test_params_desc.description->name = "Module 1";
	test_description.type = AMOD_TYPE_PROCESS;
	test_params_desc.description->functions = NULL;
	test_params_desc.description = &test_description;

	ret = amod_open(&test_params_desc, (struct amod_configuration *)&config, inst_name, &handle,
			(struct amod_context *)&context);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (-22)");
}

ZTEST(suite_a_mod, test_open_bad_state)
{
	int ret;
	char *inst_name = "TEST open";
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct amod_parameters test_params = {.description = &test_description};
	struct mod_config configuration = {0};
	struct amod_configuration *config = (struct amod_configuration *)&configuration;
	struct amod_handle handle = {0};
	struct mod_context context = {0};

	handle.state = AMOD_STATE_CONFIGURED;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-114)");
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state");

	handle.state = AMOD_STATE_RUNNING;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-114)");
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Open returns with incorrect state");

	handle.state = AMOD_STATE_STOPPED;
	ret = amod_open(&test_params, config, inst_name, &handle, (struct amod_context *)&context);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (-114)");
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Open returns with incorrect state");
}

ZTEST(suite_a_mod, test_open_null)
{
	int ret;
	char *inst_name = "TEST open";
	struct amod_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
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
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Open returns with incorrect state");
}

ZTEST_SUITE(suite_a_mod, NULL, NULL, NULL, NULL, NULL);
