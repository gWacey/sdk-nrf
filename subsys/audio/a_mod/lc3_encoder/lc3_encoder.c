/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "lc3_encoder.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "channel_assignment.h"
#include "pcm_stream_channel_modifier.h"
#include "aobj_api.h"
#include "amod_api.h"
#include "LC3API.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lc3_encoder, 4); /* CONFIG_LC3_ENCODER_LOG_LEVEL); */

/**
 * @brief Number of micro seconds in a second.
 *
 */
#define LC3_ENCODER_US_IN_A_SECOND (1000000)

/**
 * @brief Table of the LC3 encoder module functions.
 *
 */
struct amod_functions lc3_enc_functions = {
	/**
	 * @brief  Function to an open the LC3 encoder module.
	 */
	.open = lc3_enc_open,

	/**
	 * @brief  Function to close the LC3 encoder module.
	 */
	.close = lc3_enc_close,

	/**
	 * @brief  Function to set the configuration of the LC3 encoder module.
	 */
	.configuration_set = lc3_enc_configuration_set,

	/**
	 * @brief  Function to get the configuration of the LC3 encoder module.
	 */
	.configuration_get = lc3_enc_configuration_get,

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
	 * @brief The core data processing function in the LC3 encoder module.
	 */
	.data_process = lc3_enc_data_process,
};

/**
 * @brief The set-up parameters for the LC3 encoder.
 *
 */
struct amod_description lc3_enc_dept = { .name = "LC3 Encoder",
					 .type = AMOD_TYPE_PROCESSOR,
					 .functions = (struct amod_functions *)&lc3_enc_functions };

/**
 * @brief A private pointer to the LC3 encoder set-up parameters.
 *
 */
struct amod_description *lc3_enc_description = &lc3_enc_dept;

/**
 * @brief Open an instance of the LC3 encoder
 *
 */
int lc3_enc_open(struct handle *handle, struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	struct lc3_encoder_configuration *config =
		(struct lc3_encoder_configuration *)configuration;
	uint8_t enc_sample_rates = 0;
	uint8_t unique_session = 0;
	LC3FrameSize_t framesize;

	/* Clear the context */
	memset(ctx, 0, sizeof(struct lc3_encoder_context));

	/* Set unique session to 0 for using the default sharing memory setting.
	 *
	 * This could lead to higher heap consumtion, but is able to manipulate
	 * different sample rate setting between encoder/encoder.
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

	/* Check supported sample rates for encoder */
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_8KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_8_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_16KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_16_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_24KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_24_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_32KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_32_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_441KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_441_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_48KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_48_KHZ;
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

	ret = LC3Initialize(enc_sample_rates, enc_sample_rates, framesize, unique_session, NULL,
			    NULL);

	return ret;
}

/**
 * @brief  Function close an instance of the LC3 encoder.
 *
 */
int lc3_enc_close(struct handle *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)ctx->lc3_enc_channel;

	/* Free encoder memory */
	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		if (enc_handles[i] != NULL) {
			LC3EncodeSessionClose(enc_handles[i]);
			enc_handles = NULL;
		}
	}

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 encoder.
 *
 */
int lc3_enc_configuration_set(struct handle *handle, struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	struct lc3_encoder_configuration *config =
		(struct lc3_encoder_configuration *)configuration;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)ctx->lc3_enc_channel;
	LC3FrameSize_t framesize;
	uint16_t enc_pcm_bytes_req;

	if (ctx->enc_handles_count) {
		LOG_ERR("LC3 encoder instance %s already initialised", hdl->name);
		return -EALREADY;
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

	if (config->bitrate == 0) {
		LOG_ERR("LC3 encoder bitrate is 0 for module %s", hdl->name);
		return -EINVAL;
	} else if (config->bitrate <= ENC_BITRATE_WRN_LVL_LOW) {
		LOG_WRN("LC3 encoder bitrate: %d : likely too low for module %s", config->bitrate,
			hdl->name);
	} else if (config->bitrate >= ENC_BITRATE_WRN_LVL_HIGH) {
		LOG_WRN("LC3 encoder bitrate: %d : likely too high for module %s", config->bitrate,
			hdl->name);
	}

	enc_pcm_bytes_req =
		LC3PCMBuffersize(config->sample_rate, config->bit_depth, framesize, &ret);
	if (enc_pcm_bytes_req == 0) {
		LOG_ERR("Required PCM bytes to LC3 encode instance %s is zero", hdl->name);
		return -EPERM;
	}

	for (uint8_t i = 0; i < config->number_channels; i++) {
		enc_handles[i] = LC3EncodeSessionOpen(config->sample_rate, config->bit_depth,
						      framesize, NULL, NULL, &ret);
		if (ret) {
			LOG_ERR("Failed to initialise channel %d of LC3 encode instance %s, ret %d",
				i, hdl->name, ret);
			return ret;
		}
		ctx->enc_handles_count += 1;
	}

	ctx->pcm_bytes_req = enc_pcm_bytes_req;

	memcpy(&ctx->config, config, sizeof(struct lc3_encoder_configuration));

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 encoder.
 *
 */
int lc3_enc_configuration_get(struct handle *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	struct lc3_encoder_configuration *config =
		(struct lc3_encoder_configuration *)configuration;

	memcpy(config, &ctx->config, sizeof(struct lc3_encoder_configuration));

	/* Configure encoder */
	LOG_DBG("LC3 encode module %s configuration: %dHz %dbits %dus %d channel(s) mapped as 0x%X",
		hdl->name, config->sample_rate, config->bit_depth, config->duration_us,
		config->number_channels, config->channel_map);

	return 0;
}

/**
 * @brief Process an audio data block in an instance of the LC3 encoder.
 *
 */
int lc3_enc_data_process(struct handle *handle, struct aobj_block *block_in,
			 struct aobj_block *block_out)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)ctx->lc3_enc_channel;
	size_t session_in_size;
	size_t data_out_size;
	uint8_t *data_in;
	uint8_t *data_out;
	uint32_t encodeBitrate;
	uint8_t *deinterleaved_pcm;
	uint8_t pcm_block[ENC_PCM_NUM_BYTES_MULTI_CHAN] = { 0 };
	size_t pcm_block_size;
	uint16_t bytes_read = 0;

	if (block_in->data_type != AOBJ_TYPE_PCM) {
		LOG_DBG("Input to LC3 encoder module %s in not PCM data", hdl->name);
		return -EINVAL;
	}

	if (block_in->format.number_channels > ctx->config.number_channels) {
		LOG_ERR("Too many input audio channels");
		return -EINVAL;
	}

	/* Since LC3 is a single channel codec, we must split the
	 * stereo PCM stream if the data is channel interleaved
	 */
	if (block_in->format.interleaved == AOBJ_INTERLEAVED &&
	    block_in->format.number_channels > 1) {
		ret = pscm_deinterleave(block_in->data, block_in->data_size,
					block_in->format.bits_per_sample,
					block_in->format.number_channels, &pcm_block[0],
					&pcm_block_size);
		if (ret) {
			return ret;
		}

		deinterleaved_pcm = &pcm_block[0];
	} else {
		deinterleaved_pcm = block_in->data;
		pcm_block_size = block_in->data_size;
	}

	session_in_size = pcm_block_size / block_in->format.number_channels;
	if (session_in_size < ctx->pcm_bytes_req) {
		LOG_ERR("Too few PCM samples to encode. Bytes required %d, input frame is %d",
			ctx->pcm_bytes_req, session_in_size);
		return -EINVAL;
	}

	data_out = (uint8_t *)block_out->data;
	data_out_size = block_out->data_size;

	if (block_in->bitrate == LC3_USE_BITRATE_FROM_INIT) {
		encodeBitrate = ctx->config.bitrate;
	} else {
		encodeBitrate = block_in->bitrate;
	}

	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		data_in = (uint8_t *)block_in->data + (session_in_size * i);

		LC3EncodeInput_t LC3EncodeInput = { .PCMData = (void *)data_in,
						    .PCMDataLength = session_in_size,
						    .encodeBitrate = encodeBitrate,
						    .bytesRead = bytes_read };

		LC3EncodeOutput_t LC3EncodeOutput = { .outputData = data_in,
						      .outputDataLength = data_out_size,
						      .bytesWritten = 0 };

		if (!enc_handles[i]) {
			LOG_ERR("LC3 enc ch:%d is not initialized", i);
			return -EPERM;
		}

		ret = LC3EncodeSessionData(enc_handles[i], &LC3EncodeInput, &LC3EncodeOutput);
		if (ret) {
			LOG_ERR("Encoder %s for channel %d failed, ret %d", hdl->name, i, ret);
			return ret;
		}

		if (session_in_size != bytes_read) {
			LOG_WRN("Encoder %s bad bytes read %d session size %d ", hdl->name, i,
				bytes_read, session_in_size);
		}

		data_out_size -= LC3EncodeOutput.bytesWritten;
		data_out += LC3EncodeOutput.bytesWritten;
	}

	block_out->data_size = block_out->data_size - data_out_size;
	block_out->data_type = AOBJ_TYPE_LC3;
	block_out->format = block_in->format;
	block_out->format.interleaved = AOBJ_DEINTERLEAVED;
	block_out->bitrate = encodeBitrate;
	block_out->bad_frame = false;
	block_out->last_flag = false;
	block_out->user_data = NULL;

	return 0;
}
