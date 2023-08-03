/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_DECODER_H_
#define _LC3_DECODER_H_

#include "audio_defines.h"
#include "ablk_api.h"
#include "amod_api.h"
#include "LC3API.h"

#if (CONFIG_SW_CODEC_LC3)
#define LC3_MAX_FRAME_SIZE_MS	10
#define LC3_ENC_MONO_FRAME_SIZE (CONFIG_LC3_BITRATE * LC3_MAX_FRAME_SIZE_MS / (8 * 1000))

#define LC3_PCM_NUM_BYTES_MONO                                                                     \
	(CONFIG_AUDIO_SAMPLE_RATE_HZ * CONFIG_AUDIO_BIT_DEPTH_OCTETS * LC3_MAX_FRAME_SIZE_MS / 1000)
#define LC3_ENC_TIME_US 3000
#define LC3_DEC_TIME_US 1500
#else
#define LC3_ENC_MONO_FRAME_SIZE 0
#define LC3_PCM_NUM_BYTES_MONO	0
#define LC3_ENC_TIME_US		0
#define LC3_DEC_TIME_US		0
#endif /* CONFIG_SW_CODEC_LC3 */

/* Max will be used when multiple codecs are supported */
#define DEC_TIME_US		     MAX(LC3_DEC_TIME_US, 0)
#define DEC_PCM_NUM_BYTES_MONO	     MAX(LC3_PCM_NUM_BYTES_MONO, 0)
#define DEC_PCM_NUM_BYTES_MULTI_CHAN (DEC_PCM_NUM_BYTES_MONO * CONFIG_LC3_DEC_CHANNELS_MAX)

/**
 * @brief Private pointer to the module's parameters.
 */
extern struct amod_description *lc3_dec_description;

/**
 * @brief Private pointer to the decoders handle.
 */
struct lc3_decoder_handle;

/**
 * @brief The module configuration structure.
 */
struct lc3_decoder_configuration {
	/* Sample rate for the decoder instance */
	uint32_t sample_rate;

	/* Bit depth for this decoder instance */
	uint32_t bit_depth;

	/* Maximum bit rate for this decoder instance */
	uint32_t max_bitrate;

	/* The packing format for the output PCM buffer */
	enum aobj_interleaved packing;

	/* Frame duration for this decoder instance */
	uint32_t duration_us;

	/* Number of channels for this decoder instance */
	uint32_t number_channels;

	/* Channel map for this decoder instance */
	uint32_t channel_map;
};

/**
 * @brief  Private module context.
 */
struct lc3_decoder_context {
	/* Array of decoder channel handles */
	struct lc3_decoder_handle *lc3_dec_channel[AUDIO_CH_NUM];

	/* Number of decoder channel handles */
	uint32_t dec_handles_count;

	/* The decoder cnfiguration */
	struct lc3_decoder_configuration config;

	/* Minimum coded bytes required for this decoder instance */
	uint16_t coded_bytes_req;

	/* Audio samples per frame */
	size_t samples_per_frame;

	/* Number of successive frames to which PLC has been applied */
	uint16_t plc_count;
};

/**
 * @brief Size of the decoder's private context.
 */
#define LC3_DECODER_CONTEXT_SIZE (sizeof(struct lc3_decoder_context))

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_t2_open(struct amod_handle_private *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_t2_close(struct amod_handle_private *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_t2_configuration_set(struct amod_handle_private *handle,
				 struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_t2_configuration_get(struct amod_handle_private *handle,
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
int lc3_dec_t2_data_process(struct amod_handle_private *handle, struct aobj_block *block_in,
			    struct aobj_block *block_out);

#endif /* _LC3_DECODER_H_ */
