/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _DUMMY_PRIVATE_H_
#define _DUMMY_PRIVATE_H_

#include "dummy.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include "aobj_api.h"
#include "amod_api.h"
#include "amod_api_private.h"

#define DUMMY_MODULE_LAST_BYTES_NUM (10)

/**
 * @brief  Private module context.
 *
 */
struct dummy_context {
	/* Array of data to test with */
	uint8_t dummy_data[DUMMY_MODULE_LAST_BYTES_NUM];

	/*! The decoder cnfiguration */
	struct dummy_configuration config;
};

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int dummy_query_resource(struct amod_configuration *configuration);

/**
 * @brief  Function for opening a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int dummy_open(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function close an open module.
 *
 * @param handle  A pointer to the modules handle.
 *
 * @return 0 if successful, error value
 */
int dummy_close(struct amod_handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules configuration to set.
 *
 * @return 0 if successful, error value
 */
int dummy_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A pointer to the modules handle.
 * @param configuration  A pointer to the modules current configuration.
 *
 * @return 0 if successful, error value
 */
int dummy_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration);

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
int dummy_data_process(struct amod_handle *handle, struct aobj_object *object_in,
		       struct aobj_object *object_out);

#endif /* _DUMMY_PRIVATE_H_ */
