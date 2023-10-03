/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>

#include "common.h"
#include "audio_module.h"

const char *test_instance_name = "Test instance";
const char *test_string = "This is a test string";
const uint32_t test_uint32 = 0XDEADBEEF;

/**
 * Test function to set a module's context.
 */
void test_context_set(struct mod_context *ctx, struct mod_config const *const config)
{
	memcpy(&ctx->test_string, test_string, sizeof(test_string));
	ctx->test_uint32 = test_uint32;
	memcpy(&ctx->config, config, sizeof(struct mod_config));
}

/**
 * Test function to open a module.
 */
int test_open_function(struct audio_module_handle_private *handle,
		       struct audio_module_configuration const *const configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	test_context_set(ctx, config);
	memcpy(&ctx->test_string, test_string, sizeof(test_string));
	ctx->test_uint32 = test_uint32;
	memcpy(&ctx->config, config, sizeof(struct mod_config));

	return 0;
}

/**
 * Test function to close a module.
 */
int test_close_function(struct audio_module_handle_private *handle)
{
	ARG_UNUSED(handle);

	return 0;
}

/**
 * Test function to configure a module.
 */
int test_config_set_function(struct audio_module_handle_private *handle,
			     struct audio_module_configuration const *const configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	memcpy(&ctx->config, config, sizeof(struct mod_config));

	return 0;
}

/**
 * Test function to get the configuration of a module.
 */
int test_config_get_function(struct audio_module_handle_private const *const handle,
			     struct audio_module_configuration *configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;
	struct mod_config *config = (struct mod_config *)configuration;

	memcpy(config, &ctx->config, sizeof(struct mod_config));

	return 0;
}

/**
 * Test stop/start function of a module.
 */
int test_stop_start_function(struct audio_module_handle_private *handle)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct mod_context *ctx = (struct mod_context *)hdl->context;

	ctx->test_string = test_string;
	ctx->test_uint32 = test_uint32;

	return 0;
}

/**
 * Test process data function of a module.
 */
int test_data_process_function(struct audio_module_handle_private *handle,
			       struct audio_data const *const audio_data_rx,
			       struct audio_data *audio_data_tx)
{
	ARG_UNUSED(handle);

	memcpy(audio_data_tx, audio_data_rx, sizeof(struct audio_data));
	memcpy(audio_data_tx->data, audio_data_rx->data, audio_data_rx->data_size);
	audio_data_tx->data_size = audio_data_rx->data_size;

	return 0;
}
