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
#include "channel_assignment.h"
#include "pcm_stream_channel_modifier.h"
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
 * @brief Interleave a channel into a buffer of Nchannels of PCM
 *
 * @param[in]	input			Pointer to the single channel input buffer.
 * @param[in]	input_size		Number of bytes in input. Must be divisible by two.
 * @param[in]	channel			Channel to interleave into.
 * @param[in]	pcm_bit_depth	Bit depth of PCM samples (16, 24, or 32).
 * @param[out]	output			Pointer to the output start of the multi-channel output.
 * @param[out]	output_chans	Number of output channels in the output buffer.
 */
void interleave(void const *const input, size_t input_size, uint8_t channel, uint8_t pcm_bit_depth,
		void *output, uint8_t output_chans)
{
	uint32_t samples_per_channel;
	uint8_t bytes_per_sample = pcm_bit_depth / 8;
	size_t step;
	char *pointer_input;
	char *pointer_output;

	samples_per_channel = input_size / bytes_per_sample;
	step = bytes_per_sample * output_chans;
	pointer_input = (char *)input;
	pointer_output = (char *)&output[channel];

	for (uint32_t i = 0; i < samples_per_channel; i++) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output++ = *pointer_input++;
		}

		pointer_output += step;
	}
}

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
static int lc3_dec_t2_open(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
static int lc3_dec_t2_close(struct handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
static int lc3_dec_t2_configuration_set(struct handle *handle,
					struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
static int lc3_dec_t2_configuration_get(struct handle *handle,
					struct amod_configuration *configuration);

/**
 * @brief This processes the input block into the output block.
 *
 * @param handle	 A handle to this module instance
 * @param block_in   Pointer to the input audio block or NULL for an input module
 * @param block_out  Pointer to the output audio block or NULL for an output module
 *
 * @return 0 if successful, error value
 */
static int lc3_dec_t2_data_process(struct handle *handle, struct aobj_block *block_in,
				   struct aobj_block *block_out);
/**
 * @brief Table of the LC3 decoder module functions.
 *
 */
struct amod_functions lc3_dec_t2_functions = {
	/**
	 * @brief  Function to an open the LC3 decoder module.
	 */
	.open = lc3_dec_t2_open,

	/**
	 * @brief  Function to close the LC3 decoder module.
	 */
	.close = lc3_dec_t2_close,

	/**
	 * @brief  Function to set the configuration of the LC3 decoder module.
	 */
	.configuration_set = lc3_dec_t2_configuration_set,

	/**
	 * @brief  Function to get the configuration of the LC3 decoder module.
	 */
	.configuration_get = lc3_dec_t2_configuration_get,

	/**
	 * @brief Start a module processing data.
	 *
	 */
	.start = NULL,

	/**
	 * @brief Pause a module processing data.
	 *
	 */
	.stop = NULL,

	/**
	 * @brief The core data processing function in the LC3 decoder module.
	 */
	.data_process = lc3_dec_t2_data_process,
};

/**
 * @brief The set-up parameters for the LC3 decoder.
 *
 */
struct amod_description lc3_dec_dept = {.name = "LC3 Dcoder",
					.type = AMOD_TYPE_PROCESSOR,
					.functions =
						(struct amod_functions *)&lc3_dec_t2_functions};

/**
 * @brief A private pointer to the LC3 decoder set-up parameters.
 *
 */
struct amod_description *lc3_dec_description = &lc3_dec_dept;

/**
 * @brief Open an instance of the LC3 decoder
 *
 */
static int lc3_dec_t2_open(struct handle *handle, struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	uint8_t enc_sample_rates = 0;
	uint8_t dec_sample_rates = 0;
	uint8_t unique_session = 0;
	LC3FrameSize_t framesize;

	/* Clear the context */
	memset(ctx, 0, sizeof(struct lc3_decoder_context));

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
static int lc3_dec_t2_close(struct handle *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)ctx->lc3_dec_channel;

	/* Free decoder memory */
	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		if (dec_handles[i] != NULL) {
			LC3DecodeSessionClose(dec_handles[i]);
			dec_handles = NULL;
		}
	}

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
static int lc3_dec_t2_configuration_set(struct handle *handle,
					struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)ctx->lc3_dec_channel;
	LC3FrameSize_t framesize;
	uint16_t coded_bytes_req;

	if (ctx->dec_handles_count) {
		LOG_ERR("LC3 decoder instance %s already initialised", hdl->name);
		return -EALREADY;
	}

	/* Free previous decoder memory */
	for (uint8_t i = 0; i < ctx->config.number_channels; i++) {
		if (dec_handles[i] != NULL) {
			LC3DecodeSessionClose(dec_handles[i]);
			dec_handles[i] = NULL;
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

	coded_bytes_req = LC3BitstreamBuffersize(ctx->config.sample_rate, ctx->config.max_bitrate,
						 framesize, &ret);
	if (coded_bytes_req == 0) {
		LOG_ERR("Required coded bytes to LC3 encode instance %s is zero", hdl->name);
		return -EPERM;
	}

	for (uint8_t i = 0; i < config->number_channels; i++) {
		dec_handles[i] = LC3DecodeSessionOpen(config->sample_rate, config->bit_depth,
						      framesize, NULL, NULL, &ret);
		if (ret) {
			LOG_ERR("LC3 decoder channel %d failed to initialise for module %s", i,
				hdl->name);
			return ret;
		}

		ctx->dec_handles_count += 1;
	}

	memcpy(&ctx->config, config, sizeof(struct lc3_decoder_configuration));

	ctx->coded_bytes_req = coded_bytes_req;
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
static int lc3_dec_t2_configuration_get(struct handle *handle,
					struct amod_configuration *configuration)
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
 * @brief Process an audio data block in an instance of the LC3 decoder.
 *
 */
static int lc3_dec_t2_data_process(struct handle *handle, struct aobj_block *block_in,
				   struct aobj_block *block_out)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)ctx->lc3_dec_channel;
	LC3BFI_t frame_status;
	uint16_t plc_counter = 0;
	size_t session_in_size;
	uint8_t *data_in;
	uint8_t *data_out;
	uint8_t temp_pcm[LC3_PCM_NUM_BYTES_MONO];

	if (block_in->data_type != AOBJ_TYPE_LC3) {
		LOG_DBG("Input to LC3 decoder module %s in not LC3 data", hdl->name);
		return -EINVAL;
	}

	if (block_in->bad_frame) {
		frame_status = BadFrame;
	} else {
		frame_status = GoodFrame;
		ctx->plc_count = 0;
	}

	session_in_size = block_in->data_size / block_in->format.number_channels;
	if (session_in_size < ctx->coded_bytes_req) {
		LOG_ERR("Too few coded bytes to decode. Bytes required %d, input framesize is %d",
			ctx->coded_bytes_req, session_in_size);
		return -EINVAL;
	}

	if (ctx->packing == AOBJ_INTERLEAVED) {
		data_out = &temp_pcm[0];
	} else {
		data_out = (uint8_t *)block_out->data;
	}

	for (uint8_t i = 0; i < block_in->format.number_channels; i++) {
		data_in = (uint8_t *)block_in->data + (session_in_size * i);

		LC3DecodeInput_t LC3DecodeInput = {
			.inputData = data_in, .inputDataLength = session_in_size, frame_status};
		LC3DecodeOutput_t LC3DecodeOutput = {.PCMData = data_out,
						     .PCMDataLength = ctx->samples_per_frame,
						     .bytesWritten = 0,
						     .PLCCounter = plc_counter};

		if (dec_handles[i] == NULL) {
			LOG_DBG("LC3 dec ch:%d is not initialized", i);
			continue;
		}

		ret = LC3DecodeSessionData(dec_handles[i], &LC3DecodeInput, &LC3DecodeOutput);
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

		if (ctx->packing == AOBJ_INTERLEAVED) {
			size_t output_size;

			interleave(data_out, LC3DecodeOutput.bytesWritten,
				   block_in->format.bits_per_sample, i, block->data)

		} else {
			data_out += LC3DecodeOutput.bytesWritten;
		}
	}

	ctx->plc_count = plc_counter;

	block_out->data_size = block_out->data_size - (block_out->data - data_out);
	block_out->data_type = AOBJ_TYPE_PCM;
	block_out->format = block_in->format;
	block_out->format.interleaved = ctx->packing;
	block_out->bitrate = block_in->bitrate;
	block_out->bad_frame = block_in->bad_frame;
	block_out->last_flag = false;
	block_out->user_data = NULL;

	return 0;
}
