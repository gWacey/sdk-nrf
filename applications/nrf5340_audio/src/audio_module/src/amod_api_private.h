/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _AMOD_API_PRIVATE_H_
#define _AMOD_API_PRIVATE_H_

#include "amod_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include "data_fifo.h"

/**
 * @brief Private pointer to a modules context.
 *
 */
struct amod_context;

/**
 * @brief Private pointer to a modules functions.
 *
 */
struct _amod_functions {
	/** @brief  Function for querying the resources required for a module.
	 *
	 * @param configuration  A pointer to the modules configuration
	 * @param size			 The size of the memory buffer required
	 *
	 * @return If successful the value will be 0 or greater, otherwise error value
	 */
	int (*query_resource)(struct amod_configuration *configuration);

	/**
	 * @brief  Function for opening a module.
	 *
	 *
	 * @param handle  A handle to this module instance
	 *
	 * @return 0 if successful, error value
	 */
	int (*open)(struct _amod_handle *handle, struct amod_configuration *configuration);

	/**
	 * @brief  Function to close an open module.
	 *
	 * @param handle  A handle to this module instance
	 *
	 * @return 0 if successful, error value
	 */
	int (*close)(struct _amod_handle *handle);

	/**
	 * @brief  Function to set the configuration of a module.
	 *
	 * @param handle		 A handle to this module instance
	 * @param configuration  A pointer to the modules configuration to set
	 *
	 * @return 0 if successful, error value
	 */
	int (*configuration_set)(struct _amod_handle *handle,
				 struct amod_configuration *configuration);

	/**
	 * @brief  Function to get the configuration of a module.
	 *
	 * @param handle		 A handle to this module instance
	 * @param configuration  A pointer to the modules current configuration
	 *
	 * @return 0 if successful, error value
	 */
	int (*configuration_get)(struct _amod_handle *handle,
				 struct amod_configuration *configuration);

	/**
	 * @brief Start a module processing data.
	 *
	 * @param handle  The handle for the module to start
	 *
	 * @return 0 if successful, error value
	 */
	int (*start)(struct amod_handle *handle);

	/**
	 * @brief Pause a module processing data.
	 *
	 * @param handle  The handle for the module to pause
	 *
	 * @return 0 if successful, error value
	 */
	int (*pause)(struct amod_handle *handle);

	/**
	 * @brief The core data processing function for the module. Can be either an
	 *		input or output or processor module type.
	 *
	 * @param handle	 A handle to this module instance
	 * @param object_in  Pointer to the input audio object or NULL for an input module
	 * @param object_out Pointer to the output audio object or NULL for an output module
	 *
	 * @return 0 if successful, error value
	 */
	int (*data_process)(struct _amod_handle *handle, struct aobj_object *object_in,
			    struct aobj_object *object_out);
};

/**
 * @brief Module's thread configuration structure.
 *
 */
struct amod_thread_configuration {
	/*! Flag to indicate thread configuration has been set */
	bool set;

	/*! Thread stack */
	uint8_t *stack;

	/* Thread stack size */
	size_t stack_size;

	/*! Thread priority */
	enum amod_id priority;

	/*! Number of data input messages that can be queued */
	uint32_t in_msg_num;

	/*! Number of data output messages that can be queued */
	uint32_t out_msg_num;

	/*! Number of data buffers that can be queued */
	uint32_t data_num;

	/*! Size of a single data buffer */
	uint32_t data_size;
};

/**
 * @brief A modules minimum description.
 *
 */
struct _amod_parameters {
	/*! The module base name*/
	char *name;

	/*! The module type */
	enum amod_type type;

	/*! A pointer to the functions to implment the module */
	struct amod_functions *functions;

	/*! Thread configuration */
	struct amod_thread_configuration thread_config;
};

/**
 * @brief Private module handle.
 *
 */
struct _amod_handle {
	/*! Unique name of this module instance */
	char name[AMOD_NAME_SIZE];

	/*! Base module name */
	char base_name[AMOD_NAME_SIZE];

	/*! The module type */
	enum amod_type type;

	/*! Pointer to the specific module functions */
	struct amod_functions *functions;

	/*! Current state of the module */
	enum amod_state state;

	/*! Previous state of the module */
	enum amod_state previous_state;

	/*! Thread ID */
	k_tid_t thread_id;

	/*! Thread data */
	struct k_thread thread_data;

	/*! Pointer to a modules data input fifo */
	struct data_fifo in_msg;

	/*! Pointer to a modules data input fifo */
	struct data_fifo out_msg;

	/*! Pointer to a modules data buffer slab */
	struct k_mem_slab data_slab;

	/*! Size of a data output buffer */
	size_t data_size;

	/*! Array of module handles for sending the output data to */
	struct amod_handle *output_handles[CONFIG_AMOD_OUTPUT_MODULE_NUM];

	/*! Number of modules to send the output to */
	int output_handles_num;

	/*! Modules thread configuration */
	struct amod_thread_configuration thread;

	/*! Private context for the module */
	struct amod_context *context;

	/*! Size of the private context for the module */
	size_t context_size;
};

#endif _AMOD_API_PRIVATE_H_
