/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _AUDIO_I2S_IO_H_
#define _AUDIO_I2S_IO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "amod_api.h"
#include "aobj_api.h"

/**
 * @brief The module configuration structure.
 */
struct audio_i2s_io_configuration {
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
 * @brief Tone private context
 *
 */
struct tone_context {
	/* Flag to indicate if a tone is active or not */
	bool active;

	/* Buffer which can hold max 1 period test tone at 100 Hz */
	uint16_t test_buf[CONFIG_AUDIO_SAMPLE_RATE_HZ / 100];

	/* Size of the test tone */
	size_t test_size;
};

/**
 * @brief  Private module context.
 */
struct audio_i2s_io_context {
	/* The decoder cnfiguration */
	struct audio_i2s_io_configuration config;

	/* Tone context */
	struct tone_context tone;
};

/**
 * @brief Size of the I2S's private context.
 */
#define AUDIO_I2S_IO_CONTEXT_SIZE (sizeof(struct audio_i2s_io_context))

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_query_resource(struct amod_configuration *configuration);

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_open(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_close(struct handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_configuration_set(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_configuration_get(struct handle *handle, struct amod_configuration *configuration);

/**
 * @brief Start a module processing data.
 *
 * @param handle  The handle for the module to start
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_start(struct handle *handle);

/**
 * @brief Pause a module processing data.
 *
 * @param handle  The handle for the module to pause
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_pause(struct handle *handle);

/**
 * @brief This processes the data block into the block out.
 *
 * @param handle	 A handle to this module instance
 * @param block_in   Pointer to the input audio block or NULL for an input module
 * @param block_out  Pointer to the output audio block or NULL for an output module
 *
 * @return 0 if successful, error value
 */
int audio_i2s_io_data_process(struct handle *handle, struct aobj_block *block_in,
			      struct aobj_block *block_out);

#endif /* _AUDIO_I2S_IO_H_ */
