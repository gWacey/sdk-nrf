/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _AUDIO_MODULE_TEMPLATE_H_
#define _AUDIO_MODULE_TEMPLATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "audio_defines.h"
#include "audio_module.h"

#define AUDIO_MODULE_TEMPLATE_LAST_BYTES (10)

/**
 * @brief Private pointer to the module's parameters.
 *
 */
extern struct audio_module_description *audio_module_template_description;

/**
 * @brief The module configuration structure.
 *
 */
struct audio_module_template_configuration {
	/*! The rate. */
	uint32_t rate;

	/*! the depth. */
	uint32_t depth;

	/*! A string .*/
	char *some_text;
};

/**
 * @brief  Private module context.
 *
 */
struct audio_module_template_context {
	/* Array of data to illustrate the data process function. */
	uint8_t audio_module_template_data[AUDIO_MODULE_TEMPLATE_LAST_BYTES];

	/*! The decoder configuration. */
	struct audio_module_template_configuration config;
};

/**
 * @brief Function for opening a module with the specified initial configuration.
 *
 * @param handle[in/out]     The handle to the module instance.
 * @param configuration[in]  Pointer to the desired initial configuration to set.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_open(struct audio_module_handle_private *handle,
			       struct audio_module_configuration const *const configuration);

/**
 * @brief Function to close an open module.
 *
 * @param handle[in/out]  The handle to the module instance.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_close(struct audio_module_handle_private *handle);

/**
 * @brief Function to reconfigure a module after it has been opened with its initial
 *        configuration.
 *
 * @param handle[in/out]     The handle to the module instance.
 * @param configuration[in]  Pointer to the desired configuration to set.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_configuration_set(
	struct audio_module_handle_private *handle,
	struct audio_module_configuration const *const configuration);

/**
 * @brief Function to get the configuration of a module.
 *
 * @param handle[in]          The handle to the module instance.
 * @param configuration[out]  Pointer to the module's current configuration.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_configuration_get(struct audio_module_handle_private const *const handle,
					    struct audio_module_configuration *configuration);

/**
 * @brief Start a module.
 *
 * @param handle[in/out]  The handle for the module to start.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_start(struct audio_module_handle_private *handle);

/**
 * @brief Stop a module.
 *
 * @param handle[in/out]  The handle for the module to stop.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_stop(struct audio_module_handle_private *handle);

/**
 * @brief The core data processing function for the module. Can be either an
 *		  input, output or input/output module type.
 *
 * @param handle[in/out]      The handle to the module instance.
 * @param audio_data_rx[in]   Pointer to the input audio data or NULL for an input module.
 * @param audio_data_tx[out]  Pointer to the output audio data or NULL for an output module.
 *
 * @return 0 if successful, error otherwise.
 */
int audio_module_template_data_process(struct audio_module_handle_private *handle,
				       struct audio_data const *const audio_data_in,
				       struct audio_data *audio_data_out);

#endif /* _AUDIO_MODULE_TEMPLATE_H_ */
