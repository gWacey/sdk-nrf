/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "lc3_decoder.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "aobj_api.h"
#include "amod_api.h"
#include "LC3API.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lc3_decoder, 4); /* CONFIG_LC3_DECODER_LOG_LEVEL); */

/**
 * @brief Number of micro seconds in a second.
 *
 */
#define LC3_DECODER_US_IN_A_SECOND (1000000)

/**
 * @brief Table of the LC3 decoder module functions.
 *
 */
struct amod_functions lc3_dec_functions = {
	/**
	 * @brief  Function for querying the resources required for LC3 decoder.
	 */
	.query_resource = lc3_dec_query_resource,

	/**
	 * @brief  Function to an open the LC3 decoder module.
	 */
	.open = lc3_dec_open,

	/**
	 * @brief  Function to close the LC3 decoder module.
	 */
	.close = lc3_dec_close,

	/**
	 * @brief  Function to set the configuration of the LC3 decoder module.
	 */
	.configuration_set = lc3_dec_configuration_set,

	/**
	 * @brief  Function to get the configuration of the LC3 decoder module.
	 */
	.configuration_get = lc3_dec_configuration_get,

	/**
	 * @brief Start a module processing data.
	 *
	 */
	.start = NULL,

	/**
	 * @brief Pause a module processing data.
	 *
	 */
	.pause = NULL,

	/**
	 * @brief The core data processing function in the LC3 decoder module.
	 */
	.data_process = lc3_dec_data_process,
};

/**
 * @brief The set-up parameters for the LC3 decoder.
 *
 */
struct amod_description lc3_dec_dept = { .name = "LC3 Dcoder",
					 .type = AMOD_TYPE_PROCESSOR,
					 .functions = (struct amod_functions *)&lc3_dec_functions };

/**
 * @brief A private pointer to the LC3 decoder set-up parameters.
 *
 */
struct amod_description *lc3_dec_description = &lc3_dec_dept;

/**
 * @brief  Function for querying the resources required for the LC3 decoder
 *		   module with the given configuration.
 *
 */
int lc3_dec_query_resource(struct amod_configuration *configuration)
{
	return sizeof(struct lc3_decoder_context);
}

/**
 * @brief Open an instance of the LC3 decoder
 *
 */
int lc3_dec_open(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	uint8_t enc_sample_rates = 0;
	uint8_t dec_sample_rates = 0;
	uint8_t unique_session = 0;
	LC3FrameSize_t framesize;
	int ret;

	/* Set unique session to 0 for using the default sharing memory setting.
	 *
	 * This could lead to higher heap consumtion, but is able to manipulate
	 * different sample rate setting between encoder/decoder.
	 */

	/* Check supported sample rates for encoder */
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_8KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_8_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_16KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_16_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_24KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_24_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_32KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_32_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_441KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_441_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_48KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_48_KHZ;
	}

	/* Check supported sample rates for decoder */
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_8KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_8_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_16KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_16_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_24KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_24_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_32KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_32_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_441KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_441_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_48KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_48_KHZ;
	}

	switch (config->duration_us) {
	case 7500:
		framesize = LC3FrameSize7_5Ms;
		break;
	case 10000:
		framesize = LC3FrameSize10Ms;
		break;
	default:
		LOG_ERR("Unsupported framesize: %d", config->duration_us);
		return -EINVAL;
	}

	ret = LC3Initialize(enc_sample_rates, dec_sample_rates, framesize, unique_session, NULL,
			    NULL);

	return ret;
}

/**
 * @brief  Function close an instance of the LC3 decoder.
 *
 */
int lc3_dec_close(struct amod_handle *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;

	/* Free decoder memory */
	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		if (ctx->lc3_dec_channel[i] != NULL) {
			LC3DecodeSessionClose(ctx->lc3_dec_channel[i]);
			ctx->lc3_dec_channel[i] = NULL;
		}
	}

	/* Clear handle */
	ctx = NULL;

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
int lc3_dec_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3FrameSize_t framesize;
	int ret;

	/* Free previous decoder memory */
	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		if (ctx->lc3_dec_channel[i] != NULL) {
			LC3DecodeSessionClose(ctx->lc3_dec_channel[i]);
			ctx->lc3_dec_channel[i] = NULL;
		}
	}

	switch (config->duration_us) {
	case 7500:
		framesize = LC3FrameSize7_5Ms;
		break;
	case 10000:
		framesize = LC3FrameSize10Ms;
		break;
	default:
		LOG_ERR("Unsupported framesize: %d", config->duration_us);
		return -EINVAL;
	}

	for (uint8_t i = 0; i < config->number_channels; i++) {
		ctx->lc3_dec_channel[i] = LC3DecodeSessionOpen(
			config->sample_rate, config->bit_depth, framesize, NULL, NULL, &ret);
		if (ret) {
			LOG_ERR("LC3 decoder channel %d failed to initialise for module %s", i,
				hdl->name);
			return ret;
		}
	}

	ctx->config.number_channels = config->number_channels;

	memcpy(&ctx->config, config, sizeof(struct lc3_decoder_configuration));

	ctx->samples_per_frame =
		(ctx->config.duration_us * ctx->config.sample_rate) / LC3_DECODER_US_IN_A_SECOND;

	/* Configure decoder */
	LOG_DBG("LC3 decode module %s configuration: %dHz %dbits %dus %d channel(s)", hdl->name,
		ctx->config.sample_rate, ctx->config.bit_depth, ctx->config.duration_us,
		ctx->config.number_channels);

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
int lc3_dec_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;

	memcpy(config, &ctx->config, sizeof(struct lc3_decoder_configuration));

	/* Configure decoder */
	LOG_DBG("LC3 decode module %s configuration: %dHz %dbits %dus %d channel(s) mapped as 0x%X",
		hdl->name, config->sample_rate, config->bit_depth, config->duration_us,
		config->number_channels, config->channel_map);

	return 0;
}

/**
 * @brief Process an audio data object in an instance of the LC3 decoder.
 *
 */
int lc3_dec_data_process(struct amod_handle *handle, struct aobj_object *object_in,
			 struct aobj_object *object_out)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3BFI_t frame_status;
	uint16_t plc_counter = 0;
	size_t session_in_size;
	size_t data_out_size;
	uint8_t *data_in;
	uint8_t *data_out;
	int ret;

	if (object_in->data_type != AOBJ_CODING_TYPE_LC3) {
		LOG_DBG("Input to LC3 decoder module %s in not LC3 data", hdl->name);
		return -EINVAL;
	}

	if (object_in->bad_frame) {
		frame_status = BadFrame;
	} else {
		frame_status = GoodFrame;
	}

	session_in_size = object_in->data_size / ctx->config.number_channels;
	data_out_size = ctx->samples_per_frame * ctx->config.number_channels;

	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		data_in = (uint8_t *)object_in->data + (session_in_size * i);
		data_out = (uint8_t *)object_out->data + (ctx->samples_per_frame * i);

		LC3DecodeInput_t LC3DecodeInput = { .inputData = data_in,
						    .inputDataLength = session_in_size,
						    frame_status };
		LC3DecodeOutput_t LC3DecodeOutput = { .PCMData = data_out,
						      .PCMDataLength = ctx->samples_per_frame,
						      .bytesWritten = 0,
						      .PLCCounter = plc_counter };

		if (ctx->lc3_dec_channel[i] == NULL) {
			LOG_DBG("LC3 dec ch:%d is not initialized", i);
			continue;
		}

		ret = LC3DecodeSessionData(ctx->lc3_dec_channel[i], &LC3DecodeInput,
					   &LC3DecodeOutput);
		if (ret) {
			/* handle error */
			LOG_DBG("Error in decoder, ret: %d", ret);
			continue;
		}

		if (LC3DecodeOutput.bytesWritten != ctx->samples_per_frame) {
			/* handle error */
			LOG_DBG("Error in decoder, output incorrect size %d when should "
				"be %d",
				LC3DecodeOutput.bytesWritten, ctx->samples_per_frame);

			/* Clear this channel as it is not correct */
			memset(data_out, 0, LC3DecodeOutput.bytesWritten);
		}
	}

	return 0;
}
