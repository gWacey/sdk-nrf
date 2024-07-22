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

/**
 * @brief Helper function to validate the modules mix option.
 *
 * @param mix_opt  [in]  The module's mix_opt.
 *
 * @return true if mix_opt is invalid, false otherwise.
 */
static bool mix_option_invalid(enum tone_gen_mix_option mix_opt)
{
	if (mix_opt == TONE_GEN_NO_MIX || mix_opt == TONE_GEN_MIX_ALL) {
		return false;
	}

	return true;
}

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

	if (mix_option_invalid(config->mix_opt)) {
		LOG_WRN("Tone mix option is invalid %d for module %s", 
		config->mix_opt, hdl->name);
		return -EINVAL;

	}

	memset(&ctx->meta, 0, sizeof(struct audio_metadata));
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
		"Carrier = %d bits  Amplitude = %.4lf  Mixer option = %d",
		hdl->name, ctx->config.frequency_hz, ctx->meta.sample_rate_hz, ctx->meta.bits_per_sample,
		ctx->meta.carried_bits_per_sample, (double)ctx->config.amplitude, ctx->config.mix_opt);

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
	uint8_t *data_out = audio_data_out->data;
	int step_out = ctx->carried_bytes_per_sample - ctx->bytes_per_sample;
	size_t full_cycles;
	size_t bytes_remaining;
	uint8_t *tone_input = (uint8_t *)ctx->tone_buffer;
	size_t size;
	int8_t channels;

	if (audio_data_in->data_size > audio_data_out->data_size ||
		audio_data_in->meta.data_coding != PCM) {
		LOG_WRN("Data input missmatch for module %s", hdl->name);
		return -EINVAL;
	}

	audio_module_number_channels_calculate(audio_data_in->meta.locations, &channels);

	/* Generate the tone into the tone buffer if the specification has changed. */
	if (audio_data_in->meta.sample_rate_hz != ctx->meta.sample_rate_hz) {
		ret = tone_gen(ctx->tone_buffer, &ctx->cycle_bytes_num, ctx->config.frequency_hz,
			       ctx->meta.sample_rate_hz, ctx->config.amplitude);
		if (ret) {
			LOG_WRN("Failed to generate a tone for module %s", hdl->name);
			return ret;
		}

		ctx->bytes_per_sample = ctx->meta.bits_per_sample / 8;
		ctx->carried_bytes_per_sample = ctx->meta.carried_bits_per_sample / 8;

		memcpy(&ctx->meta, &audio_data_in->meta, sizeof(struct audio_metadata));
	}

	if (ctx->bytes_per_sample != ctx->carried_bytes_per_sample) {
		memset(audio_data_out->data, 0, audio_data_out->data_size);
	}

	if (ctx->byte_remain_index) {
		for (size_t i = ctx->byte_remain_index; i < ctx->cycle_bytes_num; i += sizeof(int16_t)) {
			for (size_t k = 0; k < sizeof(int16_t); k++) {
				*data_out++ = *tone_input++;
			}

			data_out += step_out;
		}
	}

	if (audio_data_in->data_size == 0) {
		size = audio_data_out->data_size;
	} else {
		size = audio_data_in->data_size;
	}

	full_cycles = (size - ctx->byte_remain_index) / ctx->cycle_bytes_num;
	bytes_remaining = (size - ctx->byte_remain_index) % ctx->cycle_bytes_num;

	for (size_t i = 0; i < full_cycles; i++) {
		uint8_t *tone_input = (uint8_t *)ctx->tone_buffer;

		for (size_t j = 0; j < ctx->cycle_bytes_num; j += sizeof(int16_t)) {
			for (size_t k = 0; k < sizeof(int16_t); k++) {
				*data_out++ = *tone_input++;
			}

			data_out += step_out;
		}
	}

	if (bytes_remaining) {
		for (size_t i = 0; i < bytes_remaining; i += sizeof(int16_t)) {
			for (size_t k = 0; k < sizeof(int16_t); k++) {
				*data_out++ = *tone_input++;
			}

			data_out += step_out;
		}
	}

	ctx->byte_remain_index = bytes_remaining;

	memcpy(&audio_data_out->meta, &audio_data_in->meta, sizeof(struct audio_metadata));

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
