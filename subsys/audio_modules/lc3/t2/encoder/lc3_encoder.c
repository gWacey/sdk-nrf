/*
 * Copyright(c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_modules/lc3_encoder.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>

#include "audio_defines.h"
#include "audio_module/audio_module.h"
#include "LC3API.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(t2_lc3_encoder, CONFIG_AUDIO_MODULE_LC3_ENCODER_LOG_LEVEL);

/**
 * @brief Number of micro seconds in a second.
 *
 */
#define LC3_ENCODER_US_IN_A_SECOND (1000000)

/**
 * @brief Uninterleave a channel from a buffer of N channels of PCM
 *
 * @note: The uninterleaver can not be executed inplace (i.e. input != output)
 *
 * @param[in]	input			Pointer to the multi channel input buffer.
 * @param[in]	input_size		Number of bytes in input. Must be divisible by two.
 * @param[in]	input_channels	Number of input channels in the input buffer.
 * @param[in]	channel			Channel to interleave into.
 * @param[in]	pcm_bit_depth	Bit depth of PCM samples (8, 16, 24, or 32).
 * @param[out]	output			Pointer to the single channel output.
 * @param[in]	output_size		Number of bytes in output. Must be divisible by two and
 *                              at least (input_size / output_channels).
 * @param[out]	out_used_bytes  Pointer to the number of valid bytes within the returned 
 *                              output buffer.
 *
 * @return 0 if successful, error value
 */
static int uninterleave(void const *const input, size_t input_size, uint8_t input_channels,
			  uint8_t channel, uint8_t pcm_bit_depth, void *output, size_t output_size,
			  uint32_t *out_used_bytes)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;
	size_t step;
	uint8_t *pointer_input;
	uint8_t *pointer_output;
	uint32_t used_size = 0;

	if (input == NULL || input_size == 0 || channel > input_channels || pcm_bit_depth == 0 ||
	    output == NULL || output_size == 0 || input_channels == 0) {
		return -EINVAL;
	}

	if (output_size < (input_size / input_channels)) {
		LOG_DBG("Output buffer too small to uninterleave input into");
		return -EINVAL;
	}

	step = bytes_per_sample * (input_channels - 1);
	pointer_input = (uint8_t *)input + (step * channel);
	pointer_output = (uint8_t *)output;

	for (size_t i = 0; i < input_size; i += (step + bytes_per_sample)) {
		for (size_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output++ = *pointer_input++;
		}

		pointer_input += step;
	}

	*out_used_bytes = used_size;

	return 0;
}

/**
 * @brief  Open the LC3 module.
 *
 * @param handle         A pointer to the audio module's handle.
 * @param configuration  A pointer to the audio module's configuration to set.
 *
 * @return 0 if successful, error value
 */
static int lc3_enc_t2_open(struct audio_module_handle_private *handle,
			   struct audio_module_configuration const *const configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;

	/* Clear the context */
	memset(ctx, 0, sizeof(struct lc3_encoder_context));

	LOG_DBG("Open LC3 encoder module");

	return 0;
}

/**
 * @brief  Close the LC3 audio module.
 *
 * @param handle  A pointer to the audio module's handle.
 *
 * @return 0 if successful, error value
 */
static int lc3_enc_t2_close(struct audio_module_handle_private *handle)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)&ctx->lc3_enc_channel[0];
	int8_t number_channels;

	audio_module_number_channels_calculate(ctx->config.locations, &number_channels);

	/* Close ENCODER sessions */
	for (uint8_t i = 0; i < number_channels; i++) {
		if (enc_handles[i] != NULL) {
			LC3EncodeSessionClose(enc_handles[i]);
			enc_handles[i] = NULL;
		}
	}

	return 0;
}

/**
 * @brief  Set the configuration of an audio module.
 *
 * @param handle         A pointer to the audio module's handle.
 * @param configuration  A pointer to the audio module's configuration to set.
 *
 * @return 0 if successful, error value
 */
static int
lc3_enc_t2_configuration_set(struct audio_module_handle_private *handle,
			     struct audio_module_configuration const *const configuration)
{
	int ret;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	struct lc3_encoder_configuration *config =
		(struct lc3_encoder_configuration *)configuration;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)&ctx->lc3_enc_channel[0];
	LC3FrameSize_t framesize;
	uint16_t coded_frame_bytes;
	int8_t number_channels;

	/* Need to validate the config parameters here before proceeding */

	audio_module_number_channels_calculate(config->locations, &number_channels);

	/* Free previous encoder memory */
	for (uint8_t i = 0; i < number_channels; i++) {
		if (enc_handles[i] != NULL) {
			LC3EncodeSessionClose(enc_handles[i]);
			enc_handles[i] = NULL;
		}
	}

	switch (config->data_len_us) {
	case 7500:
		framesize = LC3FrameSize7_5Ms;
		break;
	case 10000:
		framesize = LC3FrameSize10Ms;
		break;
	default:
		LOG_ERR("Unsupported framesize: %d", config->data_len_us);
		return -EINVAL;
	}

	coded_frame_bytes = LC3BitstreamBuffersize(config->sample_rate_hz, config->bitrate_bps_max,
						 framesize, &ret);
	if (coded_frame_bytes == 0) {
		LOG_ERR("Required coded bytes to LC3 instance %s is zero", hdl->name);
		return -EPERM;
	}

	for (uint8_t i = 0; i < number_channels; i++) {
		enc_handles[i] =
			LC3EncodeSessionOpen(config->sample_rate_hz, config->bits_per_sample,
					     framesize, NULL, NULL, &ret);
		if (ret) {
			LOG_ERR("LC3 ENCODER channel %d failed to initialise for module %s", i,
				hdl->name);
			return ret;
		}

		LOG_DBG("LC3 encode module %s session %d: %dus %dbits", hdl->name, i,
			config->data_len_us, config->bits_per_sample);

		ctx->enc_handles_count += 1;
	}

	memcpy(&ctx->config, config, sizeof(struct lc3_encoder_configuration));

	ctx->coded_frame_bytes = coded_frame_bytes;
	ctx->sample_frame_req = ((ctx->config.data_len_us * ctx->config.sample_rate_hz) /
				   LC3_ENCODER_US_IN_A_SECOND) *
				  (ctx->config.carried_bits_per_sample / 8);

	LOG_DBG("LC3 encode module %s requires %d sample bytes to produce %d encoded bytes",
		hdl->name, ctx->sample_frame_req, ctx->coded_frame_bytes);

	/* Configure ENCODER */
	LOG_DBG("LC3 encode module %s configuration: %d Hz %d bits (sample bits %d) "
		"%d us %d "
		"channel(s)",
		hdl->name, ctx->config.sample_rate_hz, ctx->config.carried_bits_per_sample,
		ctx->config.bits_per_sample, ctx->config.data_len_us, number_channels);

	return 0;
}

/**
 * @brief  Get the current configuration of an audio module.
 *
 * @param handle         A pointer to the audio module's handle.
 * @param configuration  A pointer to the audio module's current configuration.
 *
 * @return 0 if successful, error value
 */
static int lc3_enc_t2_configuration_get(struct audio_module_handle_private const *const handle,
					struct audio_module_configuration *configuration)
{
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	struct lc3_encoder_configuration *config =
		(struct lc3_encoder_configuration *)configuration;
	memcpy(config, &ctx->config, sizeof(struct lc3_encoder_configuration));

	/* Configure ENCODER */
	LOG_DBG("LC3 encode module %s configuration: %dHz %dbits (sample bits %d) %dus channel(s) "
		"mapped as 0x%X",
		hdl->name, config->sample_rate_hz, config->carried_bits_per_sample,
		config->bits_per_sample, config->data_len_us, config->locations);

	return 0;
}

/**
 * @brief This processes the input audio data into the output audio data.
 *
 * @param handle	      A handle to this audio module's instance
 * @param audio_data_in   Pointer to the input audio data or NULL for an input module
 * @param audio_data_out  Pointer to the output audio data or NULL for an output module
 *
 * @return 0 if successful, error value
 */
static int lc3_enc_t2_data_process(struct audio_module_handle_private *handle,
				   struct audio_data const *const audio_data_in,
				   struct audio_data *audio_data_out)
{
	int ret;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;
	struct lc3_encoder_context *ctx = (struct lc3_encoder_context *)hdl->context;
	LC3EncoderHandle_t *enc_handles = (LC3EncoderHandle_t *)&ctx->lc3_enc_channel[0];
	size_t coded_out_size;
	size_t session_in_size;
	uint8_t *data_in;
	uint8_t *coded_out;
	uint8_t data_in_temp[LC3_ENCODER_PCM_NUM_BYTES_MONO];
	int8_t number_channels;
	uint32_t data_in_size;

	LOG_DBG("LC3 ENCODER module %s start process", hdl->name);

	if (audio_data_in->meta.data_coding != PCM) {
		LOG_DBG("LC3 ENCODER module %s has incorrect input data type: %d", hdl->name,
			audio_data_in->meta.data_coding);
		return -EINVAL;
	}

	if (ctx->config.locations != audio_data_in->meta.locations) {
		LOG_DBG("LC3 ENCODER module %s has incorrect channel map in the new audio_data: %d",
			hdl->name, audio_data_in->meta.locations);
		return -EINVAL;
	}

	audio_module_number_channels_calculate(ctx->config.locations, &number_channels);

	if (audio_data_in->data_size) {
		session_in_size = audio_data_in->data_size / number_channels;
		if (session_in_size < ctx->sample_frame_req) {
			LOG_ERR("Too few coded bytes to encode. Bytes required- %d, input "
				"framesize "
				"is %d",
				ctx->sample_frame_req, session_in_size);
			return -EINVAL;
		}
	} else {
		session_in_size = 0;
	}

	if (audio_data_out->data_size < ctx->coded_frame_bytes * number_channels) {
		LOG_ERR("Output buffer too small. Bytes required %d, output buffer is %d",
			(ctx->coded_frame_bytes * number_channels), audio_data_out->data_size);
		return -EINVAL;
	}

	coded_out_size = 0;
	memcpy(&audio_data_out->meta, &audio_data_in->meta, sizeof(struct audio_metadata));

	/* Should be able to encode only the channel(s) of interest here.
	 * These will be put in the first channel or channels and the location
	 * will indicate which channel(s) they are. Prior to playout (I2S or TDM)
	 * all other channels can be zeroed.
	 */
	for (uint8_t chan = 0; chan < number_channels; chan++) {
		coded_out = (uint8_t *)audio_data_out->data + (session_in_size * chan);

		if (enc_handles[chan] == NULL) {
			LOG_DBG("LC3 enc ch: %d is not initialized", chan);
			return -EINVAL;
		}

		if (ctx->config.interleaved) {
			ret = uninterleave(audio_data_in->data, audio_data_in->data_size,
					 number_channels, chan, ctx->config.bits_per_sample,
					 data_in_temp, sizeof(data_in_temp), &data_in_size);

			if (ret) {
				LOG_DBG("Failed to uninterleave input");
				return ret;
			}

			data_in = data_in_temp;

			LOG_DBG("Completed encoder PCM input uninterleaving for ch: %d", chan);
		} else {
			data_in = (uint8_t *)audio_data_in->data;
		}

		LC3EncodeInput_t LC3EncodeInput = { .PCMData = data_in,
					    .PCMDataLength = data_in_size,
					    .bytesRead = 0 };
		LC3EncodeOutput_t LC3EncodeOutput = { .outputData = coded_out,
					      .outputDataLength = ctx->coded_frame_bytes,
					      .bytesWritten = 0 };

		ret = LC3EncodeSessionData(enc_handles[chan], &LC3EncodeInput, &LC3EncodeOutput);
		if (ret) {
			/* handle error */
			LOG_DBG("Error in ENCODER, ret: %d", ret);
			return ret;
		}

		coded_out += LC3EncodeOutput.bytesWritten;
		coded_out_size += LC3EncodeOutput.bytesWritten;

		LOG_DBG("Completed LC3 encode of ch: %d", chan);
	}

	audio_data_out->data_size = coded_out_size;
	audio_data_out->meta.data_coding = LC3;

	return 0;
}

/**
 * @brief Table of the LC3 ENCODER module functions.
 */
struct audio_module_functions lc3_enc_t2_functions = {
	/**
	 * @brief  Function to an open the LC3 ENCODER module.
	 */
	.open = lc3_enc_t2_open,

	/**
	 * @brief  Function to close the LC3 ENCODER module.
	 */
	.close = lc3_enc_t2_close,

	/**
	 * @brief  Function to set the configuration of the LC3 ENCODER module.
	 */
	.configuration_set = lc3_enc_t2_configuration_set,

	/**
	 * @brief  Function to get the configuration of the LC3 ENCODER module.
	 */
	.configuration_get = lc3_enc_t2_configuration_get,

	/**
	 * @brief Start a module processing data.
	 */
	.start = NULL,

	/**
	 * @brief Pause a module processing data.
	 */
	.stop = NULL,

	/**
	 * @brief The core data processing function in the LC3 ENCODER module.
	 */
	.data_process = lc3_enc_t2_data_process};

/**
 * @brief The set-up parameters for the LC3 ENCODER.
 */
struct audio_module_description lc3_enc_t2_dept = {
	.name = "LC3 ENCODER (T2)",
	.type = AUDIO_MODULE_TYPE_IN_OUT,
	.functions = (struct audio_module_functions *)&lc3_enc_t2_functions};

/**
 * @brief A private pointer to the LC3 ENCODER set-up parameters.
 */
struct audio_module_description *lc3_encoder_description = &lc3_enc_t2_dept;
