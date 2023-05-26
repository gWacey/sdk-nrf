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
	int (*open)(struct amod_handle *handle, struct amod_configuration *configuration);

	/**
	 * @brief  Function to close an open module.
	 *
	 * @param handle  A handle to this module instance
	 *
	 * @return 0 if successful, error value
	 */
	int (*close)(struct amod_handle *handle);

	/**
	 * @brief  Function to set the configuration of a module.
	 *
	 * @param handle		 A handle to this module instance
	 * @param configuration  A pointer to the modules configuration to set
	 *
	 * @return 0 if successful, error value
	 */
	int (*configuration_set)(struct amod_handle *handle,
				 struct amod_configuration *configuration);

	/**
	 * @brief  Function to get the configuration of a module.
	 *
	 * @param handle		 A handle to this module instance
	 * @param configuration  A pointer to the modules current configuration
	 *
	 * @return 0 if successful, error value
	 */
	int (*configuration_get)(struct amod_handle *handle,
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
	int (*data_process)(struct amod_handle *handle, struct aobj_object *object_in,
			    struct aobj_object *object_out);
};

/**
 * @brief Module's thread configuration structure.
 *
 */
struct _amod_thread_configuration {
	/*! Flag to indicate thread configuration has been set */
	bool set;

	/*! Thread stack */
	uint8_t *stack;

	/* Thread stack size */
	size_t stack_size;

	/*! Thread priority */
	int priority;

	/*! Number of concurrent input messages */
	int in_msg_num;

	/*! Number of concurrent output messages */
	int out_msg_num;
};

/**
 * @brief A modules minimum description.
 *
 */
struct _amod_parameters {
	/*! The module base name */
	char *name;

	/*! The module type */
	enum amod_type type;

	/*! A pointer to the functions to implment the module */
	struct _amod_functions *functions;
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
	struct _amod_functions *functions;

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

	/* List node (next pointer) */
	sys_snode_t node;

	/*! A singley linked-list of the handles the module is connected to. */
	sys_slist_t hdl_dest_list;

	/* Mutex to make the above destinations list thread safe */
	struct k_mutex dest_mutex;

	/*! Modules thread configuration */
	struct _amod_thread_configuration thread;

	/*! Private context for the module */
	struct amod_context *context;
};

#endif /* _AMOD_API_PRIVATE_H_ */
