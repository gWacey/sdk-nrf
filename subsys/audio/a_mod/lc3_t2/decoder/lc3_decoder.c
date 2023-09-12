/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "lc3_decoder.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>

#include "audio_defines.h"
#include "ablk_api.h"
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
	.data_process = lc3_dec_t2_data_process};

/**
 * @brief The set-up parameters for the LC3 decoder.
 *
 */
struct amod_description lc3_dec_dept = {.name = "LC3 Decoder (T2)",
					.type = AMOD_TYPE_PROCESS,
					.functions =
						(struct amod_functions *)&lc3_dec_t2_functions};

/**
 * @brief A private pointer to the LC3 decoder set-up parameters.
 *
 */
struct amod_description *lc3_decoder_description = &lc3_dec_dept;

/**
 * @brief Interleave a channel into a buffer of N channels of PCM
 *
 * @param[in]	input			Pointer to the single channel input buffer.
 * @param[in]	input_size		Number of bytes in input. Must be divisible by two.
 * @param[in]	channel			Channel to interleave into.
 * @param[in]	pcm_bit_depth	Bit depth of PCM samples (16, 24, or 32).
 * @param[out]	output			Pointer to the output start of the multi-channel output.
 * @param[in]	output_size		Number of bytes in output. Must be divisible by two and
 *                              atleast (input_size * bytes_per_sample * output_channels).
 * @param[out]	output_channels	Number of output channels in the output buffer.
 *
 * @return 0 if successful, error value
 */
int interleave(void const *const input, size_t input_size, uint8_t channel, uint8_t pcm_bit_depth,
	       void *output, size_t output_size, uint8_t output_channels)
{
	uint32_t samples_per_channel;
	uint8_t bytes_per_sample = pcm_bit_depth / 8;
	size_t step;
	char *pointer_input;
	char *pointer_output;

	if (output_size < (input_size * output_channels)) {
		LOG_DBG("Output buffer too small to interleave input into");
		return -EINVAL;
	}

	samples_per_channel = input_size / bytes_per_sample;
	step = bytes_per_sample * (output_channels - 1);
	pointer_input = (char *)input;
	pointer_output = (char *)output + (bytes_per_sample * channel);

	for (uint32_t i = 0; i < samples_per_channel; i++) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output++ = *pointer_input++;
		}

		pointer_output += step;
	}

	return 0;
}

/**
 * @brief Open an instance of the LC3 decoder
 *
 */
int lc3_dec_t2_open(struct amod_handle_private *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;

	/* Clear the context */
	memset(ctx, 0, sizeof(struct lc3_decoder_context));

	return 0;
}

/**
 * @brief  Function close an instance of the LC3 decoder.
 *
 */
int lc3_dec_t2_close(struct amod_handle_private *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)&ctx->lc3_dec_channel[0];
	int8_t number_channels;

	amod_number_channels_calculate(ctx->config.channel_map, &number_channels);

	/* Close decoder sessions */
	for (uint8_t i = 0; i < number_channels; i++) {
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
int lc3_dec_t2_configuration_set(struct amod_handle_private *handle,
				 struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)&ctx->lc3_dec_channel[0];
	LC3FrameSize_t framesize;
	uint16_t coded_bytes_req;
	int8_t number_channels;

	/* Need to validate the config parameters here before proceeding */

	amod_number_channels_calculate(config->channel_map, &number_channels);

	/* Free previous decoder memory */
	for (uint8_t i = 0; i < number_channels; i++) {
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

	coded_bytes_req =
		LC3BitstreamBuffersize(config->sample_rate, config->max_bitrate, framesize, &ret);
	if (coded_bytes_req == 0) {
		LOG_ERR("Required coded bytes to LC3 instance %s is zero", hdl->name);
		return -EPERM;
	}

	for (uint8_t i = 0; i < number_channels; i++) {
		dec_handles[i] = LC3DecodeSessionOpen(config->sample_rate, config->bits_per_sample,
						      framesize, NULL, NULL, &ret);
		if (ret) {
			LOG_ERR("LC3 decoder channel %d failed to initialise for module %s", i,
				hdl->name);
			return ret;
		}

		LOG_DBG("LC3 decode module %s session %d: %dus %dbits", hdl->name, i,
			config->duration_us, config->bits_per_sample);

		ctx->dec_handles_count += 1;
	}

	memcpy(&ctx->config, config, sizeof(struct lc3_decoder_configuration));

	ctx->coded_bytes_req = coded_bytes_req;
	ctx->samples_per_frame =
		(ctx->config.duration_us * ctx->config.sample_rate) / LC3_DECODER_US_IN_A_SECOND;

	/* Configure decoder */
	LOG_DBG("LC3 decode module %s configuration: %dHz %dbits (sample bits %d) %dus %d "
		"channel(s)",
		hdl->name, ctx->config.sample_rate, ctx->config.carrier_size,
		ctx->config.bits_per_sample, ctx->config.duration_us, number_channels);

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
int lc3_dec_t2_configuration_get(struct amod_handle_private *handle,
				 struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	struct lc3_decoder_configuration *config =
		(struct lc3_decoder_configuration *)configuration;
	memcpy(config, &ctx->config, sizeof(struct lc3_decoder_configuration));

	/* Configure decoder */
	LOG_DBG("LC3 decode module %s configuration: %dHz %dbits (sample bits %d) %dus channel(s) "
		"mapped as 0x%X",
		hdl->name, config->sample_rate, config->carrier_size, config->bits_per_sample,
		config->duration_us, config->channel_map);

	return 0;
}

/**
 * @brief Process an audio data block in an instance of the LC3 decoder.
 *
 */
int lc3_dec_t2_data_process(struct amod_handle_private *handle, struct ablk_block *block_in,
			    struct ablk_block *block_out)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct lc3_decoder_context *ctx = (struct lc3_decoder_context *)hdl->context;
	LC3DecoderHandle_t *dec_handles = (LC3DecoderHandle_t *)&ctx->lc3_dec_channel[0];
	LC3BFI_t frame_status;
	uint16_t plc_counter = 0;
	size_t data_out_size;
	size_t session_in_size;
	size_t frame_size_bytes;
	uint8_t *data_in;
	uint8_t *data_out;
	uint8_t temp_pcm[LC3_PCM_NUM_BYTES_MONO];
	int8_t number_channels;

	if (block_in->data_type != ABLK_TYPE_LC3) {
		LOG_DBG("LC3 decoder module %s has incorrect input data type: %d", hdl->name,
			block_in->data_type);
		return -EINVAL;
	}

	if (block_in->bad_frame) {
		frame_status = BadFrame;
	} else {
		frame_status = GoodFrame;
		ctx->plc_count = 0;
	}

	amod_number_channels_calculate(ctx->config.channel_map, &number_channels);

	session_in_size = block_in->data_size / number_channels;
	if (session_in_size < ctx->coded_bytes_req) {
		LOG_ERR("Too few coded bytes to decode. Bytes required %d, input framesize is %d",
			ctx->coded_bytes_req, session_in_size);
		return -EINVAL;
	}

	frame_size_bytes = (ctx->samples_per_frame * (ctx->config.carrier_size / 8));

	if (block_out->data_size < frame_size_bytes * number_channels) {
		LOG_ERR("Output buffer too small. Bytes required %d, output buffer is %d",
			(frame_size_bytes * number_channels), block_out->data_size);
		return -EINVAL;
	}

	data_out = (uint8_t *)block_out->data;
	data_out_size = block_out->data_size;

	memcpy(block_out, block_in, sizeof(struct ablk_block));
	block_out->data = (void *)data_out;
	block_out->data_size = data_out_size;
	block_out->data_type = ABLK_TYPE_PCM;
	block_out->interleaved = ctx->config.interleaved;

	if (ctx->config.interleaved == ABLK_INTERLEAVED) {
		data_out = &temp_pcm[0];
	}

	data_out_size = 0;

	for (uint8_t chan = 0; chan < number_channels; chan++) {
		data_in = (uint8_t *)block_in->data + (session_in_size * chan);

		LC3DecodeInput_t LC3DecodeInput = {
			.inputData = data_in, .inputDataLength = session_in_size, frame_status};
		LC3DecodeOutput_t LC3DecodeOutput = {.PCMData = data_out,
						     .PCMDataLength = frame_size_bytes,
						     .bytesWritten = 0,
						     .PLCCounter = plc_counter};

		if (dec_handles[chan] == NULL) {
			LOG_DBG("LC3 dec ch:%d is not initialized", chan);
			return -EINVAL;
		}

		ret = LC3DecodeSessionData(dec_handles[chan], &LC3DecodeInput, &LC3DecodeOutput);
		if (ret) {
			/* handle error */
			LOG_DBG("Error in decoder, ret: %d", ret);
			return ret;
			;
		}

		if (LC3DecodeOutput.bytesWritten != frame_size_bytes) {
			/* handle error */
			LOG_DBG("Error in decoder, output incorrect size %d when should "
				"be %d",
				LC3DecodeOutput.bytesWritten, frame_size_bytes);

			/* Clear this channel as it is not correct */
			memset(data_out, 0, LC3DecodeOutput.bytesWritten);

			return -EFAULT;
		}

		if (ctx->config.interleaved == ABLK_INTERLEAVED) {
			ret = interleave(data_out, LC3DecodeOutput.bytesWritten, chan,
					 block_in->bits_per_sample, block_out->data,
					 block_out->data_size, number_channels);

			if (ret) {
				LOG_DBG("Failed to interleave output");
				return ret;
			}

		} else {
			data_out += LC3DecodeOutput.bytesWritten;
		}

		data_out_size += LC3DecodeOutput.bytesWritten;
	}

	ctx->plc_count = plc_counter;

	block_out->data_size = data_out_size;

	return 0;
}
