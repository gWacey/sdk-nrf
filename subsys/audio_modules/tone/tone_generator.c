/*
 * Copyright(c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "tone_generator.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "audio_defines.h"
#include "audio_module.h"
#include "tone.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_module_tone_generator, CONFIG_AUDIO_MODULE_TONE_GENERATOR_LOG_LEVEL);

static int audio_module_tone_gen_open(struct audio_module_handle_private *handle,
				      struct audio_module_configuration const *const configuration)

{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_generator_context *ctx =
		(struct audio_module_tone_generator_context *)hdl->context;
	struct audio_module_tone_generator_configuration *config =
		(struct audio_module_tone_generator_configuration *)configuration;

	memset(ctx, 0, sizeof(struct audio_module_tone_generator_context));
	memcpy(&ctx->config, config, sizeof(struct audio_module_tone_generator_configuration));

	LOG_DBG("Open %s module", hdl->name);

	return 0;
}

static int audio_module_tone_gen_close(struct audio_module_handle_private *handle)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_generator_context *ctx =
		(struct audio_module_tone_generator_context *)hdl->context;

	memset(ctx, 0, sizeof(struct audio_module_tone_generator_context));

	LOG_DBG("Close %s module", hdl->name);

	return 0;
}

static int audio_module_tone_gen_configuration_set(
	struct audio_module_handle_private *handle,
	struct audio_module_configuration const *const configuration)
{
	struct audio_module_tone_generator_context *config =
		(struct audio_module_tone_generator_context *)configuration;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_generator_context *ctx =
		(struct audio_module_tone_generator_context *)hdl->context;

	memcpy(&ctx->config, config, sizeof(struct audio_module_tone_generator_configuration));

	LOG_DBG("Set the configuration for %s module: Sample rate = %d  Hz  
	    Sample depth = %d bits  Carrier = %d bits  Interleved: %d",
		hdl->name, ctx->config.sample_rate_hz, ctx->config.bits_per_sample,
		ctx->config.carried_bits_pr_sample, (ctx->config.interleaved ? "YES" : "NO"));

	return 0;
}

static int
audio_module_tone_gen_configuration_get(struct audio_module_handle_private const *const handle,
					struct audio_module_configuration *configuration)
{
	struct audio_module_tone_generator_configuration *config =
		(struct audio_module_tone_generator_configuration *)configuration;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_generator_context *ctx =
		(struct audio_module_tone_generator_context *)hdl->context;

	memcpy(config, &ctx->config, sizeof(struct audio_module_tone_generator_configuration));

	LOG_DBG("Set the configuration for %s module: Sample rate = %d  Hz  
	    Sample depth = %d bits  Carrier = %d bits  Interleved: %d",
		hdl->name, config.sample_rate_hz, config.bits_per_sample,
		config.carried_bits_pr_sample, (config.interleaved ? "YES" : "NO"));

	return 0;
}

static int audio_module_tone_gen_data_process(struct audio_module_handle_private *handle,
					      struct audio_data const *const audio_data_in,
					      struct audio_data *audio_data_out)
{
	int ret;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_generator_context *ctx =
		(struct audio_module_tone_generator_context *)hdl->context;

	if (audio_data_out->meta.sample_rate_hz != ctx->config.sample_rate_hz ||
	    audio_data_out->meta.bits_per_sample != ctx->config.bits_per_sample || 
	    audio_data_out->meta.carried_bits_pr_sample != ctx->config.carried_bits_pr_sample) {
			LOG_WRN("Missmatch in the parameters for the output buffer and the module");
			return -EINVAL;
	}

	ret = tone_gen(audio_data_out->data, &audio_data_out->data_size, ctx->config.frequency, 
			ctx->config.sample_rate_hz, ctx->config.amplitude);
	if (ret) {
		LOG_WRN("Failed to generate a tone for moudle %s", hdl->name);
	}

	LOG_DBG("Process the tone into the output audio data item for %s module", hdl->name);

	return 0;
}

/**
 * @brief Table of the dummy module functions.
 */
const struct audio_module_functions audio_module_tone_generator_functions = {
	/**
	 * @brief  Function to an open the dummy module.
	 */
	.open = audio_module_tone_gen_open,

	/**
	 * @brief  Function to close the dummy module.
	 */
	.close = audio_module_tone_gen_close,

	/**
	 * @brief  Function to set the configuration of the dummy module.
	 */
	.configuration_set = audio_module_tone_gen_configuration_set,

	/**
	 * @brief  Function to get the configuration of the dummy module.
	 */
	.configuration_get = audio_module_tone_gen_configuration_get,

	/**
	 * @brief Start a module processing data.
	 */
	.start = NULL,

	/**
	 * @brief Pause a module processing data.
	 */
	.stop = NULL,

	/**
	 * @brief The core data processing function in the dummy module.
	 */
	.data_process = audio_module_tone_generator_data_process,
};

/**
 * @brief The set-up description for the tone generator module.
 */
struct audio_module_description audio_module_tone_generator_dept = {
	.name = "Tone Generator",
	.type = AUDIO_MODULE_TYPE_INPUT,
	.functions = &audio_module_tone_generator_functions};

/**
 * @brief A private pointer to the tone generator set-up parameters.
 */
struct audio_module_description *audio_module_tone_generator_description = &audio_module_tone_generator_dept;
