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
#include <contin_array.h>
#include "audio_defines.h"
#include "audio_module.h"
#include "tone.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_module_tone_generator, CONFIG_AUDIO_MODULE_TONE_GENERATOR_LOG_LEVEL);

static int audio_module_tone_gen_open(struct audio_module_handle_private *handle,
				      struct audio_module_configuration const *const configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_gen_context *ctx =
		(struct audio_module_tone_gen_context *)hdl->context;

	memset(ctx, 0, sizeof(struct audio_module_tone_gen_context));

	LOG_DBG("Open %s module", hdl->name);

	return 0;
}

static int audio_module_tone_gen_close(struct audio_module_handle_private *handle)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_gen_context *ctx =
		(struct audio_module_tone_gen_context *)hdl->context;

	memset(ctx, 0, sizeof(struct audio_module_tone_gen_context));

	LOG_DBG("Close %s module", hdl->name);

	return 0;
}

static int audio_module_tone_gen_configuration_set(
	struct audio_module_handle_private *handle,
	struct audio_module_configuration const *const configuration)
{
	struct audio_module_tone_gen_configuration *config =
		(struct audio_module_tone_gen_configuration *)configuration;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_gen_context *ctx =
		(struct audio_module_tone_gen_context *)hdl->context;

	if (config->frequency_hz > CONFIG_TONE_GENERATION_FREQUENCY_HZ_MAX ||
		config->frequency_hz < CONFIG_TONE_GENERATION_FREQUENCY_HZ_MIN) {
		LOG_WRN("Tone frequency out of range %d for module %s", 
		config->frequency_hz, hdl->name);
		return -EINVAL;
	}

	if ((double)config->amplitude > 1.0 || (double)config->amplitude < 0.0) {
		LOG_WRN("Tone amplitude out of range %.4lf for module %s", 
		(double)config->amplitude, hdl->name);
		return -EINVAL;
	}

	if ((double)config->tone_scale > 1.0 || (double)config->tone_scale < 0.0) {
		LOG_WRN("Tone amplitude out of range %.4lf for module %s", 
		(double)config->amplitude, hdl->name);
		return -EINVAL;
	}

	if ((double)config->input_scale > 1.0 || (double)config->input_scale < 0.0) {
		LOG_WRN("Tone amplitude out of range %.4lf for module %s", 
		(double)config->amplitude, hdl->name);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(struct audio_module_tone_gen_context));
	
	ctx->tone_audio_data.data = (void *)&ctx->tone_buffer;
	ctx->tone_int_scale = (uint32_t)(config->tone_scale * (float)UINT32_MAX);
	ctx->pcm_int_scale = (uint32_t)(config->input_scale * (float)UINT32_MAX);

	memcpy(&ctx->config, config, sizeof(struct audio_module_tone_gen_configuration));

	return 0;
}

static int
audio_module_tone_gen_configuration_get(struct audio_module_handle_private const *const handle,
					struct audio_module_configuration *configuration)
{
	struct audio_module_tone_gen_configuration *config =
		(struct audio_module_tone_gen_configuration *)configuration;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_gen_context *ctx =
		(struct audio_module_tone_gen_context *)hdl->context;

	memcpy(config, &ctx->config, sizeof(struct audio_module_tone_gen_configuration));

	LOG_DBG("Get the configuration for %s module: Tone frequency = %d Hz Sample rate = %d Hz  Sample depth = %d bits "
		"Carrier = %d bits  Amplitude = %.4lf  Mixer locations = 0x%8X",
		hdl->name, ctx->config.frequency_hz, ctx->tone_audio_data.meta.sample_rate_hz, ctx->tone_audio_data.meta.bits_per_sample,
		ctx->tone_audio_data.meta.carried_bits_per_sample, (double)ctx->config.amplitude, ctx->config.mix_locations);

	return 0;
}

static int audio_module_tone_gen_data_process(struct audio_module_handle_private *handle,
					      struct audio_data const *const audio_data_in,
					      struct audio_data *audio_data_out)
{
	int ret;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct audio_module_tone_gen_context *ctx =
		(struct audio_module_tone_gen_context *)hdl->context;
	uint8_t channels;
	 
	if (audio_data_in->data_size > audio_data_out->data_size ||
		audio_data_in->meta.data_coding != PCM) {
		LOG_WRN("Data input missmatch for module %s (%d %d %d)", 
		hdl->name, audio_data_in->data_size, audio_data_out->data_size, audio_data_in->meta.data_coding);
		return -EINVAL;
	}

	audio_module_number_channels_calculate(audio_data_in->meta.locations, &channels);

	/* Generate the tone into the tone buffer if the specification has changed. */
	if (audio_data_in->meta.sample_rate_hz != ctx->tone_audio_data.meta.sample_rate_hz ||
		audio_data_in->meta.bits_per_sample != ctx->tone_audio_data.meta.bits_per_sample ||
		audio_data_in->meta.carried_bits_per_sample != ctx->tone_audio_data.meta.carried_bits_per_sample ||
		ctx->tone_audio_data.meta.locations != ctx->config.mix_locations) {
		ret = tone_gen_size(ctx->tone_audio_data.data, &ctx->tone_audio_data.data_size, ctx->config.frequency_hz,
			       audio_data_in->meta.sample_rate_hz, audio_data_in->meta.bits_per_sample, 
				   audio_data_in->meta.carried_bits_per_sample, ctx->config.amplitude);
		if (ret) {
			LOG_WRN("Failed to generate a tone for module %s", hdl->name);
			return ret;
		}

		ctx->tone_audio_data.meta.sample_rate_hz = audio_data_in->meta.sample_rate_hz;
		ctx->tone_audio_data.meta.bits_per_sample = audio_data_in->meta.bits_per_sample;
		ctx->tone_audio_data.meta.carried_bits_per_sample = audio_data_in->meta.carried_bits_per_sample;
		ctx->tone_audio_data.meta.locations = ctx->config.mix_locations;

		ctx->finite_pos = 0;

		LOG_DBG("New tone at %d Hz at a sample rate %d Hz",
			  ctx->config.frequency_hz, audio_data_in->meta.sample_rate_hz);
	}

	memset(audio_data_out->data, 0, audio_data_out->data_size);
	memcpy(&audio_data_out->meta, &audio_data_in->meta, sizeof(struct audio_metadata));

	if (audio_data_in->data_size != 0 || audio_data_in->data_size < audio_data_out->data_size) { 
		audio_data_out->data_size = audio_data_in->data_size;
	}

	ret = cont_array_chans_create(audio_data_out, &ctx->tone_audio_data,
		  channels, ctx->config.interleave_output, &ctx->finite_pos);
	if (ret){
		LOG_ERR("Continuose tone array not constructed correctly: ret %d", ret);
		return ret;
	}

	if (ctx->config.mix_locations){
		LOG_DBG("Start mixer %#08X", ctx->config.mix_locations);
	}

	LOG_DBG("Process the tone into the output audio data item for %s module", hdl->name);

	return 0;
}

/**
 * @brief Table of the  module functions.
 */
const struct audio_module_functions audio_module_tone_generator_functions = {
	/**
	 * @brief  Function to an open the module.
	 */
	.open = audio_module_tone_gen_open,

	/**
	 * @brief  Function to close the module.
	 */
	.close = audio_module_tone_gen_close,

	/**
	 * @brief  Function to set the configuration of the module.
	 */
	.configuration_set = audio_module_tone_gen_configuration_set,

	/**
	 * @brief  Function to get the configuration of the module.
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
	 * @brief The core data processing function in the module.
	 * 
	 * @note This only generates/mixes non-interleaved tone buffers.
	 */
	.data_process = audio_module_tone_gen_data_process,
};

/**
 * @brief The set-up description for the tone generator module.
 */
struct audio_module_description audio_module_tone_generator_dept = {
	.name = "Tone Generator",
	.type = AUDIO_MODULE_TYPE_IN_OUT,
	.functions = &audio_module_tone_generator_functions};

/**
 * @brief A private pointer to the tone generator set-up parameters.
 */
struct audio_module_description *audio_module_tone_gen_description =
	&audio_module_tone_generator_dept;
 