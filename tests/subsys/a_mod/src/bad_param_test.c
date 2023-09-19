/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include "fakes.h"
#include <errno.h>

#include "audio_module.h"
#include "common.h"

const struct audio_module_functions mod_1_functions = {.open = NULL,
						       .close = NULL,
						       .configuration_set = NULL,
						       .configuration_get = NULL,
						       .start = NULL,
						       .stop = NULL,
						       .data_process = NULL};

ZTEST(suite_a_mod_bad_param, test_number_channels_calculate_null)
{
	int ret;

	ret = audio_module_number_channels_calculate(1, NULL);
	zassert_equal(ret, -EINVAL,
		      "Calculate number of channels function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);
}

ZTEST(suite_a_mod_bad_param, test_state_get_state)
{
	int ret;
	struct audio_module_handle handle = {0};
	enum audio_module_state state;

	state = AMOD_STATE_UNDEFINED;
	handle.state = AMOD_STATE_UNDEFINED - 1;
	ret = audio_module_state_get(NULL, &state);
	zassert_equal(ret, -EINVAL, "Get state function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);
	zassert_equal(state, AMOD_STATE_UNDEFINED,
		      "Get state function has changed the state returned: %d", state);

	state = AMOD_STATE_UNDEFINED;
	handle.state = AMOD_STATE_STOPPED + 1;
	ret = audio_module_state_get(NULL, &state);
	zassert_equal(ret, -EINVAL, "Get state function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);
	zassert_equal(state, AMOD_STATE_UNDEFINED,
		      "Get state function has changed the state returned: %d", state);
}

ZTEST(suite_a_mod_bad_param, test_state_get_null)
{
	int ret;
	struct audio_module_handle handle = {0};
	enum audio_module_state state;

	ret = audio_module_state_get(NULL, &state);
	zassert_equal(ret, -EINVAL, "Get state function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	ret = audio_module_state_get(&handle, NULL);
	zassert_equal(ret, -EINVAL, "Get state function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);
}

ZTEST(suite_a_mod_bad_param, test_names_get_null)
{
	int ret;
	struct audio_module_handle handle = {0};
	char *base_name;
	char instance_name[CONFIG_AUDIO_MODULE_NAME_SIZE];

	ret = audio_module_names_get(NULL, &base_name, &instance_name[0]);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	ret = audio_module_names_get(&handle, NULL, &instance_name[0]);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	ret = audio_module_names_get(&handle, &base_name, NULL);
	zassert_equal(ret, -EINVAL, "Get names function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_names_get(&handle, &base_name, &instance_name[0]);
	zassert_equal(ret, -ENOTSUP, "Get names function did not return successfully (%d): ret %d",
		      -ENOTSUP, ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED,
		      "Get names returns with incorrect state: %d", handle.state);
}

ZTEST(suite_a_mod_bad_param, test_data_tx_bad_state)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};
	struct audio_module_handle handle = {.description = &test_description,
					     .previous_state = AMOD_STATE_CONFIGURED};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block = {0};

	test_block.data = &test_data[0];
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description.type = AMOD_TYPE_INPUT;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description.type = AMOD_TYPE_PROCESS;

	handle.state = AMOD_STATE_UNDEFINED;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle.state = AMOD_STATE_CONFIGURED;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle.state = AMOD_STATE_STOPPED;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);
}

ZTEST(suite_a_mod_bad_param, test_data_tx_null)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};
	struct audio_module_handle handle = {.description = &test_description,
					     .previous_state = AMOD_STATE_CONFIGURED,
					     .state = AMOD_STATE_RUNNING};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block = {0};

	test_block.data = &test_data[0];
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx(NULL, &test_block, NULL);
	zassert_equal(ret, -EINVAL, "Data TX function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_data_tx(&handle, NULL, NULL);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block.data = NULL;
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block.data = &test_data[0];
	test_block.data_size = 0;

	ret = audio_module_data_tx(&handle, &test_block, NULL);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_data_rx_bad_state)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module RX", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};
	struct audio_module_handle handle = {.description = &test_description,
					     .previous_state = AMOD_STATE_CONFIGURED,
					     .state = AMOD_STATE_RUNNING};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block = {0};

	test_block.data = &test_data[0];
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data RX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description.type = AMOD_TYPE_OUTPUT;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data RX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description.type = AMOD_TYPE_PROCESS;

	handle.state = AMOD_STATE_UNDEFINED;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data RX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle.state = AMOD_STATE_CONFIGURED;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data RX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle.state = AMOD_STATE_STOPPED;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data RX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);
}

ZTEST(suite_a_mod_bad_param, test_data_rx_null)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};
	struct audio_module_handle handle = {.description = &test_description,
					     .previous_state = AMOD_STATE_CONFIGURED,
					     .state = AMOD_STATE_RUNNING};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block = {0};

	test_block.data = &test_data[0];
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_rx(NULL, &test_block, K_NO_WAIT);
	zassert_equal(ret, -EINVAL, "Data RX function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_data_rx(&handle, NULL, K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data RX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block.data = NULL;
	test_block.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data RX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block.data = &test_data[0];
	test_block.data_size = 0;

	ret = audio_module_data_rx(&handle, &test_block, K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data RX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_data_tx_rx_bad_state)
{
	int ret;
	struct audio_module_description test_description_tx = {
		.name = "Module TX", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};
	struct audio_module_description test_description_rx = {
		.name = "Module RX", .type = AMOD_TYPE_PROCESS, .functions = NULL};
	struct audio_module_handle handle_tx = {.description = &test_description_tx,
						.previous_state = AMOD_STATE_CONFIGURED,
						.state = AMOD_STATE_RUNNING};
	struct audio_module_handle handle_rx = {.description = &test_description_rx,
						.previous_state = AMOD_STATE_CONFIGURED,
						.state = AMOD_STATE_RUNNING};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block_tx = {0};
	struct ablk_block test_block_rx = {0};

	test_block_tx.data = &test_data[0];
	test_block_tx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;
	test_block_rx.data = &test_data[0];
	test_block_rx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description_tx.type = AMOD_TYPE_INPUT;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description_tx.type = AMOD_TYPE_PROCESS;
	test_description_rx.type = AMOD_TYPE_UNDEFINED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description_rx.type = AMOD_TYPE_OUTPUT;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	test_description_tx.type = AMOD_TYPE_PROCESS;
	test_description_rx.type = AMOD_TYPE_PROCESS;

	handle_tx.state = AMOD_STATE_UNDEFINED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle_tx.state = AMOD_STATE_CONFIGURED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle_tx.state = AMOD_STATE_STOPPED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle_tx.state = AMOD_STATE_RUNNING;
	handle_rx.state = AMOD_STATE_UNDEFINED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle_rx.state = AMOD_STATE_CONFIGURED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);

	handle_rx.state = AMOD_STATE_STOPPED;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ENOTSUP, "Data TX function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);
}

ZTEST(suite_a_mod_bad_param, test_data_tx_rx_null)
{
	int ret;
	struct audio_module_description test_description_tx = {
		.name = "Module TX", .type = AMOD_TYPE_PROCESS, .functions = NULL};
	struct audio_module_description test_description_rx = {
		.name = "Module RX", .type = AMOD_TYPE_PROCESS, .functions = NULL};
	struct audio_module_handle handle_tx = {.description = &test_description_tx,
						.previous_state = AMOD_STATE_CONFIGURED,
						.state = AMOD_STATE_RUNNING};
	struct audio_module_handle handle_rx = {.description = &test_description_rx,
						.previous_state = AMOD_STATE_CONFIGURED,
						.state = AMOD_STATE_RUNNING};
	uint8_t test_data[FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	struct ablk_block test_block_tx = {0};
	struct ablk_block test_block_rx = {0};

	test_block_tx.data = &test_data[0];
	test_block_tx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;
	test_block_rx.data = &test_data[0];
	test_block_rx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx_rx(NULL, &handle_rx, &test_block_tx, &test_block_rx, K_NO_WAIT);
	zassert_equal(ret, -EINVAL, "Data TX function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_data_tx_rx(&handle_tx, NULL, &test_block_tx, &test_block_rx, K_NO_WAIT);
	zassert_equal(ret, -EINVAL, "Data TX function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, NULL, &test_block_rx, K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, NULL, K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block_tx.data = NULL;
	test_block_tx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block_tx.data = &test_data[0];
	test_block_tx.data_size = 0;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block_tx.data = &test_data[0];
	test_block_tx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;
	test_block_rx.data = NULL;
	test_block_rx.data_size = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);

	test_block_rx.data = &test_data[0];
	test_block_rx.data_size = 0;

	ret = audio_module_data_tx_rx(&handle_tx, &handle_rx, &test_block_tx, &test_block_rx,
				      K_NO_WAIT);
	zassert_equal(ret, -ECONNREFUSED,
		      "Data TX function did not return -ECONNREFUSED (%d): ret %d", -ECONNREFUSED,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_stop_bad_state)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST stop"};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Stop returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_CONFIGURED;
	ret = audio_module_stop(&handle);
	zassert_equal(ret, -ENOTSUP, "Stop function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Stop returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_stop(&handle);
	zassert_equal(ret, -EALREADY, "Stop function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Stop returns with incorrect state: ret %d",
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_stop_null)
{
	int ret;
	struct audio_module_handle handle = {0};

	ret = audio_module_stop(NULL);
	zassert_equal(ret, -EINVAL, "Stop function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	handle.state = AMOD_STATE_RUNNING;
	ret = audio_module_start(NULL);
	zassert_equal(ret, -EINVAL, "Stop function did not return successfully (%d): ret %d",
		      -EINVAL, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Stop returns with incorrect state: %d",
		      handle.state);
}

ZTEST(suite_a_mod_bad_param, test_start_bad_state)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST start"};

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_start(&handle);
	zassert_equal(ret, -ENOTSUP, "Start function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Start returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_RUNNING;
	ret = audio_module_start(&handle);
	zassert_equal(ret, -EALREADY, "Start function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Start returns with incorrect state: %d",
		      handle.state);
}

ZTEST(suite_a_mod_bad_param, test_start_null)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST start"};

	ret = audio_module_start(NULL);
	zassert_equal(ret, -EINVAL, "Start function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_start(NULL);
	zassert_equal(ret, -EINVAL, "Start function did not return successfully (%d): ret %d",
		      -EINVAL, ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Start returns with incorrect state: %d",
		      handle.state);
}

ZTEST(suite_a_mod_bad_param, test_config_get_bad_state)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST get config"};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration get function did not return -ENOTSUP (129): ret %d", ret);
	handle.state = AMOD_STATE_UNDEFINED;
}

ZTEST(suite_a_mod_bad_param, test_config_get_null)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST get config"};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;

	ret = audio_module_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state: %d", handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state: %d", handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration get function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration get returns with incorrect state: %d", handle.state);
}

ZTEST(suite_a_mod_bad_param, test_disconnect_bad_type)
{
	int ret;
	struct audio_module_description test_description_1 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct audio_module_description test_description_2 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct audio_module_handle handle_tx = {.name = "TEST connect 1",
						.state = AMOD_TYPE_UNDEFINED,
						.description = &test_description_1};
	struct audio_module_handle handle_rx = {.name = "TEST connect 2",
						.state = AMOD_TYPE_UNDEFINED,
						.description = &test_description_2};

	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	test_description_1.type = AMOD_TYPE_PROCESS;
	test_description_1.type = AMOD_TYPE_UNDEFINED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_UNDEFINED;
	handle_rx.state = AMOD_TYPE_PROCESS;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	test_description_1.type = AMOD_TYPE_INPUT;
	test_description_1.type = AMOD_TYPE_INPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_OUTPUT;
	handle_rx.state = AMOD_TYPE_INPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_PROCESS;
	handle_rx.state = AMOD_TYPE_OUTPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_disconnect_bad_state)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct audio_module_handle handle_tx = {.name = "TEST connect 1",
						.state = AMOD_STATE_UNDEFINED,
						.description = &test_description};
	struct audio_module_handle handle_rx = {.name = "TEST connect 2",
						.state = AMOD_STATE_UNDEFINED,
						.description = &test_description};

	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_STATE_CONFIGURED;
	handle_rx.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_STATE_UNDEFINED;
	handle_rx.state = AMOD_STATE_CONFIGURED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_disconnect_null)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct audio_module_handle handle_tx = {.name = "TEST connect 1",
						.state = AMOD_STATE_CONFIGURED,
						.description = &test_description};
	struct audio_module_handle handle_rx = {.name = "TEST connect 2",
						.state = AMOD_STATE_CONFIGURED,
						.description = &test_description};

	ret = audio_module_connect(NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_connect(NULL, &handle_rx);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_connect(&handle_tx, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_connect_bad_type)
{
	int ret;
	struct audio_module_description test_description_1 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct audio_module_description test_description_2 = {
		.name = "Module 1", .type = AMOD_TYPE_UNDEFINED, .functions = NULL};

	struct audio_module_handle handle_tx = {.name = "TEST connect 1",
						.state = AMOD_TYPE_UNDEFINED,
						.description = &test_description_1};
	struct audio_module_handle handle_rx = {.name = "TEST connect 2",
						.state = AMOD_TYPE_UNDEFINED,
						.description = &test_description_2};

	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	test_description_1.type = AMOD_TYPE_PROCESS;
	test_description_1.type = AMOD_TYPE_UNDEFINED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_UNDEFINED;
	handle_rx.state = AMOD_TYPE_PROCESS;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	test_description_1.type = AMOD_TYPE_INPUT;
	test_description_1.type = AMOD_TYPE_INPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_OUTPUT;
	handle_rx.state = AMOD_TYPE_INPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);

	handle_tx.state = AMOD_TYPE_PROCESS;
	handle_rx.state = AMOD_TYPE_OUTPUT;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_connect_bad_state)
{
	int ret;
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct audio_module_handle handle_tx = {.name = "TEST connect 1",
						.state = AMOD_STATE_UNDEFINED,
						.description = &test_description};
	struct audio_module_handle handle_rx = {.name = "TEST connect 2",
						.state = AMOD_STATE_UNDEFINED,
						.description = &test_description};

	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -EINVAL (-129): ret %d", ret);

	handle_tx.state = AMOD_STATE_CONFIGURED;
	handle_rx.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -EINVAL (-129): ret %d", ret);

	handle_tx.state = AMOD_STATE_UNDEFINED;
	handle_rx.state = AMOD_STATE_CONFIGURED;
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -EINVAL (-129): ret %d", ret);
}

ZTEST(suite_a_mod_bad_param, test_connect_null)
{
	int ret;
	char *test_inst_name_1 = "TEST instance 1";
	char *test_inst_name_2 = "TEST instance 2";
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = NULL};

	struct audio_module_handle handle_tx = {0};
	struct audio_module_handle handle_rx = {0};

	memcpy(&handle_tx.name, test_inst_name_1, CONFIG_AUDIO_MODULE_NAME_SIZE);
	handle_tx.state = AMOD_STATE_CONFIGURED;
	handle_tx.description = &test_description;
	memcpy(&handle_rx.name, test_inst_name_2, CONFIG_AUDIO_MODULE_NAME_SIZE);
	handle_rx.state = AMOD_STATE_CONFIGURED;
	handle_rx.description = &test_description;

	ret = audio_module_connect(NULL, NULL);
	zassert_equal(ret, -EINVAL, "Connect function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_connect(NULL, &handle_rx);
	zassert_equal(ret, -EINVAL, "Connect function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_connect(&handle_tx, NULL);
	zassert_equal(ret, -EINVAL, "Connectt function did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, 0, "Connect function did not return 0L (0): ret %d", ret);
	ret = audio_module_connect(&handle_tx, &handle_rx);
	zassert_equal(ret, -EALREADY, "Connect function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
}

ZTEST(suite_a_mod_bad_param, test_reconfig_bad_state)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST set config"};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED,
		      "Configuration set returns with incorrect state: %d", handle.state);

	handle.state = AMOD_STATE_RUNNING;
	ret = audio_module_reconfigure(&handle, config);
	zassert_equal(ret, -ENOTSUP,
		      "Configuration set function did not return -ENOTSUP (%d): ret %d", -ENOTSUP,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING,
		      "Configuration set returns with incorrect state: %d", handle.state);
}

ZTEST(suite_a_mod_bad_param, test_reconfig_null)
{
	int ret;
	struct audio_module_handle handle = {.name = "TEST set config"};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;

	ret = audio_module_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(NULL, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state: %d", handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(NULL, config);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state: %d", handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_reconfigure(&handle, NULL);
	zassert_equal(ret, -EINVAL,
		      "Configuration set function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED,
		      "Configuration set returns with incorrect state: %d", handle.state);
}

ZTEST(suite_a_mod_bad_param, test_close_bad_state)
{
	int ret;
	char *inst_name = "TEST close";
	struct audio_module_handle handle = {0};

	memcpy(&handle.name, inst_name, sizeof(inst_name));

	handle.state = AMOD_STATE_CONFIGURED;
	ret = audio_module_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EALREADY (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Close returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_close(&handle);
	zassert_equal(ret, -ENOTSUP, "Close function did not return -ENOTSUP (%d): ret %d",
		      -ENOTSUP, ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Close returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_RUNNING;
	ret = audio_module_close(&handle);
	zassert_equal(ret, -ENOTSUP, " did not return -ENOTSUP (%d): ret %d", -ENOTSUP, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Close returns with incorrect state: %d",
		      handle.state);
}

ZTEST(suite_a_mod_bad_param, test_close_null)
{
	int ret;

	ret = audio_module_close(NULL);
	zassert_equal(ret, -EINVAL, "Close function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_open_bad_thread)
{
	int ret;
	char *inst_name = "TEST open";
	struct audio_module_description test_description = {0};
	struct audio_module_parameters test_params_thread = {.description = &test_description};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;
	struct audio_module_handle handle = {0};
	struct mod_context context = {0};
	char mod_thread_stack[TEST_MOD_THREAD_STACK_SIZE];

	test_params_thread.thread.stack = NULL;
	test_params_thread.thread.stack_size = TEST_MOD_THREAD_STACK_SIZE;
	test_params_thread.thread.priority = TEST_MOD_THREAD_PRIORITY;

	ret = audio_module_open(&test_params_thread, config, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_params_thread.thread.stack = (k_thread_stack_t *)&mod_thread_stack;
	test_params_thread.thread.stack_size = 0;
	test_params_thread.thread.priority = TEST_MOD_THREAD_PRIORITY;

	ret = audio_module_open(&test_params_thread, config, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_open_bad_description)
{
	int ret;
	char *inst_name = "TEST open";
	struct audio_module_description test_description = {0};
	struct audio_module_parameters test_params_desc = {0};
	struct mod_config config = {0};
	struct audio_module_handle handle = {0};
	struct mod_context context = {0};

	test_params_desc.description = NULL;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_description.name = "Module 1";
	test_description.type = AMOD_TYPE_UNDEFINED;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_description.name = "Module 1";
	test_description.type = -1;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_description.name = "Module 1";
	test_description.type = TEST_MOD_THREAD_PRIORITY;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_description.name = NULL;
	test_description.type = AMOD_TYPE_PROCESS;
	test_description.functions = &mod_1_functions;
	test_params_desc.description = &test_description;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	test_params_desc.description->name = "Module 1";
	test_description.type = AMOD_TYPE_PROCESS;
	test_params_desc.description->functions = NULL;
	test_params_desc.description = &test_description;

	ret = audio_module_open(&test_params_desc, (struct audio_module_configuration *)&config,
				inst_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);
}

ZTEST(suite_a_mod_bad_param, test_open_bad_state)
{
	int ret;
	char *inst_name = "TEST open";
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct audio_module_parameters test_params = {.description = &test_description};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;
	struct audio_module_handle handle = {0};
	struct mod_context context = {0};

	handle.state = AMOD_STATE_CONFIGURED;
	ret = audio_module_open(&test_params, config, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_CONFIGURED, "Open returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_RUNNING;
	ret = audio_module_open(&test_params, config, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_RUNNING, "Open returns with incorrect state: %d",
		      handle.state);

	handle.state = AMOD_STATE_STOPPED;
	ret = audio_module_open(&test_params, config, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EALREADY, "Open function did not return -EALREADY (%d): ret %d",
		      -EALREADY, ret);
	zassert_equal(handle.state, AMOD_STATE_STOPPED, "Open returns with incorrect state: %d",
		      handle.state);
}

ZTEST(suite_a_mod_bad_param, test_open_null)
{
	int ret;
	char *inst_name = "TEST open";
	struct audio_module_description test_description = {
		.name = "Module 1", .type = AMOD_TYPE_PROCESS, .functions = &mod_1_functions};
	struct audio_module_parameters test_params = {.description = &test_description};
	struct mod_config configuration = {0};
	struct audio_module_configuration *config =
		(struct audio_module_configuration *)&configuration;
	struct audio_module_handle handle = {0};
	struct mod_context context = {0};

	ret = audio_module_open(NULL, NULL, NULL, NULL, NULL);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_open(NULL, config, inst_name, (struct audio_module_context *)&context,
				&handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_open(&test_params, NULL, inst_name,
				(struct audio_module_context *)&context, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_open(&test_params, config, NULL, (struct audio_module_context *)&context,
				&handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_open(&test_params, config, inst_name, NULL, &handle);
	zassert_equal(ret, -EINVAL, "Open function did not return -EINVAL (%d): ret %d", -EINVAL,
		      ret);

	ret = audio_module_open(&test_params, config, inst_name,
				(struct audio_module_context *)&context, NULL);
	zassert_equal(ret, -EINVAL,
		      "The module open functio with NULLn did not return -EINVAL (%d): ret %d",
		      -EINVAL, ret);

	handle.state = AMOD_STATE_UNDEFINED;
	ret = audio_module_open(NULL, NULL, NULL, NULL, NULL);
	zassert_equal(ret, -EINVAL, "Open function did not return -EALREADY (%d): ret %d", -EINVAL,
		      ret);
	zassert_equal(handle.state, AMOD_STATE_UNDEFINED, "Open returns with incorrect state: %d",
		      handle.state);
}
