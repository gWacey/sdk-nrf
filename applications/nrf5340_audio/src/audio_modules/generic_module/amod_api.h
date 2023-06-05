/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _AMOD_API_H_
#define _AMOD_API_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include "data_fifo.h"
#include "aobj_api.h"

/**
 * @brief Maximum size for module naming in characters.
 *
 */
#define AMOD_NAME_SIZE (20)

/**
 * @brief Modules private handle.
 */
struct amod_handle;

/**
 * @brief Private context for the module.
 */
struct amod_context;

/**
 * @brief Modules private configuration.
 */
struct amod_configuration;

/**
 * @brief Module type relative to the module.
 */
enum amod_type {
	/*! The module type is undefined */
	AMOD_TYPE_UNDEFINED = 0,

	/*! This is an input only module */
	AMOD_TYPE_INPUT,

	/*! This is an output only module */
	AMOD_TYPE_OUTPUT,

	/*! This is an input/output processing module */
	AMOD_TYPE_PROCESSOR,
};

/**
 * @brief Module state.
 */
enum amod_state {
	/*! Module state undefined */
	AMOD_STATE_UNDEFINED = 0,

	/*! Module is in the open state */
	AMOD_STATE_OPEN,

	/*! Module is in the configured state */
	AMOD_STATE_CONFIGURED,

	/*! Module is in the running state */
	AMOD_STATE_RUNNING,

	/*! Module is in the paused state */
	AMOD_STATE_PAUSED
};

/**
 * @brief Private pointer to a modules functions.
 */
struct amod_functions {
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
	 * @param handle_tx  Pointer to the input audio object or NULL for an input module
	 * @param object_rx  Pointer to the output audio object or NULL for an output module
	 *
	 * @return 0 if successful, error value
	 */
	int (*data_process)(struct amod_handle *handle, struct aobj_object *handle_tx,
			    struct aobj_object *object_rx);
};

/**
 * @brief A modules minimum description.
 */
struct amod_description {
	/*! The module base name */
	char *name;

	/*! The module type */
	enum amod_type type;

	/*! A pointer to the functions to implment the module */
	struct amod_functions *functions;
};

/**
 * @brief Module's thread configuration structure.
 */
struct amod_thread_configuration {
	/*! Thread stack */
	k_thread_stack_t *stack;

	/*! Thread stack size */
	size_t stack_size;

	/*! Thread priority */
	int priority;

	/*! Number of concurrent input messages */
	int in_msg_num;

	/*! Number of concurrent output messages */
	int out_msg_num;
};

/**
 * @brief Module's generic set-up structure.
 */
struct amod_parameters {
	/*! The modules private description */
	struct amod_description *description;

	/*! The modules thread setting */
	struct amod_thread_configuration thread;
};

/**
 * @brief  Pointer to a response callback function for a response to a data send as
 *         supplied by the module user.
 *
 * @param handle  The handle of the module that sent the data message
 * @param object  The size of the supporting data or 0 if none
 */
typedef void (*amod_response_cb)(struct amod_handle *handle, struct aobj_object *object);

/**
 * @brief Private structure describing a data in message into the module thread.
 */
struct _amod_message {
	/*! Audio data object to input */
	struct aobj_object object;

	/*! Sending module's handle */
	struct amod_handle *tx_handle;

	/*! Callback for when the data has been consumed */
	amod_response_cb response_cb;
};

/**
 * @brief Macro to return the memory required for a single data message.
 */
#define AMOD_MSG_SIZE (WB_UP(sizeof(struct _amod_message)))

/**
 * @brief Macro to return the memory required for a single data output buffer.
 */
#define AMOD_DATA_BUF_SIZE(size) (WB_UP(size))

/**
 * @brief Macro to return the memory required for a set of data messages.
 */
#define AMOD_MSG_SET_SIZE(num) (AMOD_MSG_SIZE * (num))

/**
 * @brief Macro to return the memory required for a set of data output.
 */
#define AMOD_DATA_BUF_SET_SIZE(size, chans, num) (WB_UP((size)) * (chans) * (num))

/**
 * @brief Private module handle.
 */
struct _amod_handle {
	/*! Unique name of this module instance */
	char name[AMOD_NAME_SIZE];

	/*! The modules description */
	struct amod_description description;

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

	/*! A singley linked-list of the handles the module is connected to */
	sys_slist_t hdl_dest_list;

	/*! Number of destination modules */
	uint8_t dest_count;

	/*! Semaphore to count messages between modules */
	struct k_sem sem;

	/* Mutex to make the above destinations list thread safe */
	struct k_mutex dest_mutex;

	/* If set, return the data object on the module's output message queue */
	bool msg_out;

	/*! Modules thread configuration */
	struct amod_thread_configuration thread;

	/*! Private context for the module */
	struct amod_context *context;
};

/**
 * @brief  Function for querying the resources required for a module.
 *
 * @param parameters     Pointer to the module parameters
 * @param configuration  A pointer to the modules configuration (this must also
 *                       be passed to the #amod_set_configuration() unchanged)
 *
 * @return If successful the value will be 0 or greater, otherwise error value
 */
int amod_query_resource(struct amod_parameters *parameters,
			struct amod_configuration *configuration);

/**
 * @brief  Function for allocating memory for the module.
 *
 * @return 0 if successful, error value
 */
int amod_memory_allocate(void);

/**
 * @brief  Function for de-allocating memory for the module.
 *
 * @return 0 if successful, error value
 */
int amod_memory_deallocate(void);

/**
 * @brief  Function for opening a module.
 *
 * @param parameters     Pointer to the module set-up parameters
 * @param configuration  A pointer to the modules configuration (this must also
 *                       be passed to the #amod_set_configuration() unchanged)
 * @param name           A unique name for this instance of the module
 * @param in_msg_block   Pointer to the memory allocated for module data input messages
 * @param out_msg_block  Pointer to the memory allocated for module data output messages
 * @param data_block     Pointer to the memory allocated for module data buffers
 * @param data_size      The size of each audio data buffer in chars
 * @param data_num       The number of data buffers in the data memory block
 * @param handle         Pointer to the memory allocated for the module handle
 *
 * @return 0 if successful, error value
 */
int amod_open(struct amod_parameters *parameters, struct amod_configuration *configuration,
	      char *name, char *in_msg_block, char *out_msg_block, char *data_block,
	      size_t data_size, uint32_t data_num, struct amod_handle *handle);

/**
 * @brief  Function to close an open module.
 *
 * @param handle  A handle to this module instance
 *
 * @return 0 if successful, error value
 */
int amod_close(struct amod_handle *handle);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A handle to this module instance
 * @param configuration  A pointer to the modules configuration to set (as
 *                       passed to the modules #amod_query_resource())
 *
 * @return 0 if successful, error value
 */
int amod_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief  Function to set the configuration of a module.
 *
 * @param handle         A handle to this module instance
 * @param configuration  A pointer to the modules current configuration
 *
 * @return 0 if successful, error value
 */
int amod_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief Function to connect two modules together.
 *
 * @param handle_from  The handle for the module for output
 * @param handle_to    The handle of the module to for input, if the same as ::handle from
 *                     the data will be put on the ::handle_from output message quere.
 *
 * @return 0 if successful, error value
 */
int amod_connect(struct amod_handle *handle_from, struct amod_handle *handle_to);

/**
 * @brief Function to disconnect a module.
 *
 * @param handle             The handle for the module
 * @param handle_disconnect  The handle of the module to disconnect
 *
 * @return 0 if successful, error value
 */
int amod_disconnect(struct amod_handle *handle, struct amod_handle *handle_disconnect);

/**
 * @brief Start a module processing data.
 *
 * @param handle  The handle for the module to start
 *
 * @return 0 if successful, error value
 */
int amod_start(struct amod_handle *handle);

/**
 * @brief Pause a module processing data.
 *
 * @param handle  The handle for the module to pause
 *
 * @return 0 if successful, error value
 */
int amod_pause(struct amod_handle *handle);

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param handle       The handle for the receiving module instance
 * @param object       Pointer to the audio data object to send to the module
 * @param response_cb  A pointer to a callback to run when the buffer is
 *                     fully comsumed in a module
 *
 * @return 0 if successful, error value
 */
int amod_data_tx(struct amod_handle *handle, struct aobj_object *object,
		 amod_response_cb response_cb);

/**
 * @brief Retrieve data from the module.
 *
 * @param handle  A handle to this module instance
 * @param object  Pointer to the audio data object from the module
 * @param  timeout    Non-negative waiting period to wait for operation to complete
 *	                  (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	                  or K_FOREVER to wait as long as necessary.
 *
 * @return 0 if successful, error value
 */
int amod_data_rx(struct amod_handle *handle, struct aobj_object *object, k_timeout_t timeout);

/**
 * @brief Send and retrieve data from the module all data is consumed within
 *        the call. The input data buffer maybe released once function retrurned.
 *
 * @param handle_tx   A handle to the module to send the input object to
 * @param handle_rx   A handle to the module to receive an object from
 * @param object_tx   Pointer to the audio data object to send
 * @param object_rx   Pointer to the audio data object received
 * @param timeout     Non-negative waiting period to wait for operation to complete
 *	                  (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	                  or K_FOREVER to wait as long as necessary.
 *
 * @return 0 if successful, error value
 */
int amod_data_tx_rx(struct amod_handle handle_tx, struct amod_handle handle_rx,
		    struct aobj_object *object_tx, struct aobj_object *object_rx,
		    k_timeout_t timeout);

/**
 * @brief Helper function to configure the thread information for the module
 *        set-up parameters structure.
 *
 * @param parameters   Pointer to the module set-up parameters
 * @param description  Pointer to the modules private description
 * @param stack        Memory block for the threads stack
 * @param stack_size   Size of the threads stack
 * @param priority     Priority of the thread insatnce, one of #amod_id
 * @param in_msg_num   Number of concurrent input messages
 * @param out_msg_num  Number of concurrent output messages
 *
 * @return 0 if successful, error value
 */
int amod_parameters_configure(struct amod_parameters *parameters,
			      struct amod_description *description, k_thread_stack_t *stack,
			      size_t stack_size, int priority, int in_msg_num, int out_msg_num);

/**
 * @brief Helper function to return the base and instance names for a given
 *        module handle.
 *
 * @param handle         A handle to this module instance
 * @param base_name      A pointer to the name of the module
 * @param instance_name  A pointer to the name of the current module instance
 *
 * @return 0 if successful, error value
 */
int amod_names_get(struct amod_handle *handle, char *base_name, char *instance_name);

/**
 * @brief Helper function to return the state of a given module handle.
 *
 * @param handle  A handle to this module instance
 *
 * @return 0 if successful, error value
 */
int amod_state_get(struct amod_handle *handle);

/**
 * @brief Helper function to attach a raw audio data buffer to an audio object.
 *
 * @param object     Pointer to the audio data object to fill
 * @param data       Pointer to the raw or coded audio data buffer
 * @param data_size  Size of the raw or coded audio data buffer
 *
 * @return 0 if successful, error value
 */
int amod_object_data_attach(struct aobj_object *object, char *data, size_t data_size);

/**
 * @brief Helper function to extract the raw audio data buffer from an audio object.
 *
 * @param object     Pointer to the audio data object to fill
 * @param data       Pointer to the raw or coded audio data buffer
 * @param data_size  Size of the raw or coded audio data buffer
 *
 * @return 0 if successful, error value
 */
int amod_object_data_extract(struct aobj_object *object, char *data, size_t *data_size);

#endif /*_AMOD_API_H_ */
