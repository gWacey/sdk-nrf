/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_DECODER_H_
#define _LC3_DECODER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "amod_api.h"
#include "aobj_api.h"
#include "LC3API.h"

#define CONFIG_LC3_DECODER_MAX_CHANNELS (2)

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
	struct lc3_decoder_handle *lc3_dec_channel[CONFIG_LC3_DECODER_MAX_CHANNELS];

	/* Number of decoder channel handles */
	uint32_t dec_handles_count;

	/* The decoder cnfiguration */
	struct lc3_decoder_configuration config;

	/* Audio samples per frame */
	size_t samples_per_frame;
};

/**
 * @brief Size of the decoder's private context.
 */
#define LC3_DECODER_CONTEXT_SIZE (sizeof(struct lc3_decoder_context))

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_query_resource(struct amod_configuration *configuration);

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_open(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_close(struct amod_handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief An event will fire when a new data block is available to pull out
 *        of the audio system. This can then be called to retrieve the data.
 *
 * @param handle	 A handle to this module instance
 * @param block_in   Pointer to the input audio block or NULL for an input module
 * @param block_out  Pointer to the output audio block or NULL for an output module
 *
 * @return 0 if successful, error value
 */
int lc3_dec_data_process(struct amod_handle *handle, struct aobj_block *block_in,
			 struct aobj_block *block_out);

#endif /* _LC3_DECODER_H_ */
