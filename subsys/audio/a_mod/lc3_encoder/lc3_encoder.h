/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_ENCODER_H_
#define _LC3_ENCODER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "audio_defines.h"
#include "amod_api.h"
#include "aobj_api.h"
#include "LC3API.h"

#define ENC_BITRATE_WRN_LVL_LOW 24000
#define ENC_BITRATE_WRN_LVL_HIGH 160000

#define LC3_MAX_FRAME_SIZE_MS 10
#define LC3_ENC_MONO_FRAME_SIZE (CONFIG_LC3_BITRATE * LC3_MAX_FRAME_SIZE_MS / (8 * 1000))

#define LC3_PCM_NUM_BYTES_MONO                                                                     \
	(CONFIG_AUDIO_SAMPLE_RATE_HZ * CONFIG_AUDIO_BIT_DEPTH_OCTETS * LC3_MAX_FRAME_SIZE_MS / 1000)
#define LC3_ENC_TIME_US 3000

/* Max will be used when multiple codecs are supported */
#define ENC_MAX_FRAME_SIZE MAX(LC3_ENC_MONO_FRAME_SIZE, 0)
#define ENC_TIME_US MAX(LC3_ENC_TIME_US, 0)
#define ENC_PCM_NUM_BYTES_MONO MAX(LC3_PCM_NUM_BYTES_MONO, 0)
#define ENC_PCM_NUM_BYTES_MULTI_CHAN (LC3_PCM_NUM_BYTES_MONO * AUDIO_CH_NUM)

#define LC3_USE_BITRATE_FROM_INIT 0

/**
 * @brief Private pointer to the module's parameters.
 */
extern struct amod_description *lc3_enc_description;

/**
 * @brief Private pointer to the encoders handle.
 */
struct lc3_encoder_handle;

/**
 * @brief The module configuration structure.
 */
struct lc3_encoder_configuration {
	/* Sample rate for the encoder instance */
	uint32_t sample_rate;

	/* Bit depth for this encoder instance */
	uint32_t bit_depth;

	/* Bit rate for this encoder instance */
	uint32_t bitrate;

	/* Frame duration for this encoder instance */
	uint32_t duration_us;

	/* Number of channels for this encoder instance */
	uint32_t number_channels;

	/* Channel map for this encoder instance */
	uint32_t channel_map;
};

/**
 * @brief  Private module context.
 */
struct lc3_encoder_context {
	/* Array of encoder channel handles */
	struct lc3_encoder_handle *lc3_enc_channel[AUDIO_CH_NUM];

	/* Number of encoder channel handles */
	uint32_t enc_handles_count;

	/* The encoder cnfiguration */
	struct lc3_encoder_configuration config;

	/* PCM bytes for this encoder instance */
	uint16_t pcm_bytes_req;
};

/**
 * @brief Size of the encoder's private context.
 */
#define LC3_ENCODER_CONTEXT_SIZE (sizeof(struct lc3_encoder_context))

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_enc_query_resource(struct amod_configuration *configuration);

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_enc_open(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int lc3_enc_close(struct handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_enc_configuration_set(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int lc3_enc_configuration_get(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief This processes the input block into the output block.
 *
 * @param handle	 A handle to this module instance
 * @param block_in   Pointer to the input audio block or NULL for an input module
 * @param block_out  Pointer to the output audio block or NULL for an output module
 *
 * @return 0 if successful, error value
 */
int lc3_enc_data_process(struct handle *handle, struct aobj_block *block_in,
			 struct aobj_block *block_out);

#endif /* _LC3_ENCODER_H_ */
