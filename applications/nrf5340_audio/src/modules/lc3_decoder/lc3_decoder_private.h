/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX - License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _MODULE1_PRIVATE_H_
#define _MODULE1_PRIVATE_H_

#include "lc3_decoder.h"

#include "aobj_api.h"
#include "amod_api.h"
#include "amod_api_private.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr.h>
#include "LC3API.h"

/**
 * @brief  Private module context.
 *
 */
struct lc3_decoder_context {
	/* Array of decoder channel handles */
	LC3DecoderHandle_t *lc3_dec_channel[CONFIG_LC3_DECODER_MAX_CHANNELS];

	/*! Number of decoder channel handles */
	uint32_t dec_handles_count;

	/*! The decoder cnfiguration */
	struct lc3_decoder_configuration config;

	/*! Audio samples per frame */
	size_t samples_per_frame;
};

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_query_resource(amod_configuration configuration);

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_open(struct _amod_handle *handle, amod_configuration configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_close(struct _amod_handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_configuration_set(struct _amod_handle *handle,
			      struct lc3_dec_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int lc3_dec_configuration_get(struct _amod_handle *handle,
			      struct lc3_dec_configuration *configuration);

/**
 * @brief An event will fire when a new data block is available to pull out
 *        of the audio system. This can then be called to retrieve the data.
 *
 * @param handle	 A handle to this module instance
 * @param object_in  Pointer to the input audio object or NULL for an input module
 * @param object_out Pointer to the output audio object or NULL for an output module
 *
 * @return 0 if successful, error value
 */
int lc3_dec_process_data(struct _amod_handle *handle, struct aobj_object *object_in,
			 struct aobj_object *object_out);

#endif _MODULE1_PRIVATE_H_
