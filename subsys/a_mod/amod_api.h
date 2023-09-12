/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _AMOD_API_H_
#define _AMOD_API_H_

#include <zephyr/kernel.h>

#include "data_fifo.h"
#include "ablk_api.h"

/**
 * @brief Number of valid least significant bits in the channel map
 *
 */
#define AMOD_BITS_IN_CHANNEL_MAP (32)

/**
 * @brief Channel positions within the channel map
 *
 */
#define AMOD_CHANNEL_LEFT_FRONT	 1
#define AMOD_CHANNEL_RIGHT_FRONT 2
#define AMOD_CHANNEL_CENTRE	 4
#define AMOD_CHANNEL_LFE	 8
#define AMOD_CHANNEL_LEFT_BACK	 16
#define AMOD_CHANNEL_RIGHT_BACK	 32

/**
 * @brief Module type.
 *
 */
enum amod_type {
	/* The module type is undefined */
	AMOD_TYPE_UNDEFINED = 0,

	/* This is an input processing module.
	 * Note: An input module obtains data internally within the
	 *       module (e.g. I2S) and hence has no RX FIFO.
	 */
	AMOD_TYPE_INPUT,

	/* This is an output processing module.
	 * Note: An output module outputs data internally eithin the
	 *       module (e.g. I2S) and hence has no TX FIFO.
	 */
	AMOD_TYPE_OUTPUT,

	/* This is a processing module.
	 * Note: An processing module takes input and outputs from/to another
	 *       module, thus having RX and TX FIFOs.
	 */
	AMOD_TYPE_PROCESS
};

/**
 * @brief Module state.
 */
enum amod_state {
	/* Module state undefined */
	AMOD_STATE_UNDEFINED = 0,

	/* Module is in the configured state */
	AMOD_STATE_CONFIGURED,

	/* Module is in the running state */
	AMOD_STATE_RUNNING,

	/* Module is in the stopped state */
	AMOD_STATE_STOPPED
};

/**
 * @brief Module's private handle.
 */
struct amod_handle_private;

/**
 * @brief Private context for the module.
 */
struct amod_context;

/**
 * @brief Module's private configuration.
 */
struct amod_configuration;

/**
 * @brief Callback function for a response to a data_send as
 *        supplied by the module user.
 *
 * @param handle[in/out]  The handle of the module that sent the data message
 * @param block[in]       The audio block to operate on
 */
typedef void (*amod_response_cb)(struct amod_handle_private *handle, struct ablk_block *block);

/**
 * @brief Private pointer to a module's functions.
 */
struct amod_functions {
	/**
	 * @brief Function for opening a module.
	 *
	 * @param handle[in/out]     The handle to the module instance
	 * @param configuration[in]  Pointer to the module's current configuration
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*open)(struct amod_handle_private *handle, struct amod_configuration *configuration);

	/**
	 * @brief Function to close an open module.
	 *
	 * @param handle[in/out]  The handle to the module instance
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*close)(struct amod_handle_private *handle);

	/**
	 * @brief Function to configure a module.
	 *
	 * @param handle[in/out]     The handle to the module instance
	 * @param configuration[in]  Pointer to the module's configuration to set
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*configuration_set)(struct amod_handle_private *handle,
				 struct amod_configuration *configuration);

	/**
	 * @brief Function to get the configuration of a module.
	 *
	 * @param handle[in]          The handle to the module instance
	 * @param configuration[out]  Pointer to the module's current configuration
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*configuration_get)(struct amod_handle_private *handle,
				 struct amod_configuration *configuration);

	/**
	 * @brief Start a module.
	 *
	 * @param handle[in/out]  The handle for the module to start
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*start)(struct amod_handle_private *handle);

	/**
	 * @brief Stop a module.
	 *
	 * @param handle[in/out]  The handle for the module to stop
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*stop)(struct amod_handle_private *handle);

	/**
	 * @brief The core data processing function for the module. Can be either an
	 *		  input, output or processor module type.
	 *
	 * @param handle[in/out]  The handle to the module instance
	 * @param block_rx[in]    Pointer to the input audio block or NULL for an input module
	 * @param block_tx[out]   Pointer to the output audio block or NULL for an output module
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*data_process)(struct amod_handle_private *handle, struct ablk_block *block_rx,
			    struct ablk_block *block_tx);
};

/**
 * @brief A module's minimum description.
 */
struct amod_description {
	/* The module base name */
	char *name;

	/* The module type */
	enum amod_type type;

	/* A pointer to the functions in the module */
	const struct amod_functions *functions;
};

/**
 * @brief Module's thread configuration structure.
 */
struct amod_thread_configuration {
	/* Thread stack */
	k_thread_stack_t *stack;

	/* Thread stack size */
	size_t stack_size;

	/* Thread priority */
	int priority;

	/* A pointer to a module's data receiver FIFO, can be NULL */
	struct data_fifo *msg_rx;

	/* A pointer to a module's data transmitter FIFO, can be NULL */
	struct data_fifo *msg_tx;

	/* A pointer to the data buffer sla, can be NULL */
	struct k_mem_slab *data_slab;

	/* Size of each memory block in bytes that will be
	 * taken from the data buffer slab or 0
	 */
	size_t data_size;
};

/**
 * @brief Module's generic set-up structure.
 */
struct amod_parameters {
	/* The module's private description */
	struct amod_description *description;

	/* The module's thread setting */
	struct amod_thread_configuration thread;
};

/**
 * @brief Private module handle.
 */
struct amod_handle {
	/* The unique name of this module instance */
	char name[CONFIG_AMOD_NAME_SIZE];

	/* The module's description */
	struct amod_description *description;

	/* Current state of the module */
	enum amod_state state;

	/* Previous state of the module */
	enum amod_state previous_state;

	/* Thread ID */
	k_tid_t thread_id;

	/* Thread data */
	struct k_thread thread_data;

	/* Flag to indicate if the module should send data block to its TX FIFO */
	bool data_tx;

	/* List node (next pointer) */
	sys_snode_t node;

	/* A single linked-list of the handles the module is connected to */
	sys_slist_t hdl_dest_list;

	/* Number of destination modules */
	uint8_t dest_count;

	/* Semaphore to count messages between modules */
	struct k_sem sem;

	/* Mutex to make the above destinations list thread safe */
	struct k_mutex dest_mutex;

	/* Module's thread configuration */
	struct amod_thread_configuration thread;

	/* Private context for the module */
	struct amod_context *context;
};

/**
 * @brief Private structure describing a data_in message into the module thread.
 */
struct amod_message {
	/* Audio data block to input */
	struct ablk_block block;

	/* Sending module's handle */
	struct amod_handle *tx_handle;

	/* Callback for when the data has been consumed */
	amod_response_cb response_cb;
};

/**
 * @brief Function for opening a module.
 *
 * @param parameters[in]     Pointer to the module set-up parameters
 * @param configuration[in]  Pointer to the module's configuration
 * @param name[in]           A unique name for this instance of the module
 * @param context[in]        Pointer to the private context for the module
 * @param handle[out]        Pointer to the module's private handle
 *
 * @return 0 if successful, error otherwise
 */
int amod_open(struct amod_parameters *parameters, struct amod_configuration *configuration,
	      char *name, struct amod_context *context, struct amod_handle *handle);

/**
 * @brief Function to close an open module.
 *
 * @param handle[in/out]  The handle to the module instance
 *
 * @return 0 if successful, error otherwise
 */
int amod_close(struct amod_handle *handle);

/**
 * @brief Function to reconfigure a module.
 *
 * @param handle[in/out]     The handle to the module instance
 * @param configuration[in]  Pointer to the module's configuration to set
 *
 * @return 0 if successful, error otherwise
 */
int amod_reconfigure(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief Function to get the configuration of a module.
 *
 * @param handle[in/out]      The handle to the module instance
 * @param configuration[out]  Pointer to the module's current configuration
 *
 * @return 0 if successful, error otherwise
 */
int amod_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief Function to connect two modules together.
 *
 * @param handle_from[in/out]  The handle for the module for output
 * @param handle_to[in]        The handle of the module for input. If it is the same as handle_from
 *                             the data will be put on the handle_from->output_message_quere.
 *
 * @return 0 if successful, error otherwise
 */
int amod_connect(struct amod_handle *handle_from, struct amod_handle *handle_to);

/**
 * @brief Function to disconnect modules from each other.
 *
 * @param handle[in/out]         The handle for the module
 * @param handle_disconnect[in]  The handle of the module to disconnect
 *
 * @return 0 if successful, error otherwise
 */
int amod_disconnect(struct amod_handle *handle, struct amod_handle *handle_disconnect);

/**
 * @brief Start processing data in the module given by handle.
 *
 * @param handle[in/out]  The handle for the module to start
 *
 * @return 0 if successful, error otherwise
 */
int amod_start(struct amod_handle *handle);

/**
 * @brief Stop processing data in the module given by handle.
 *
 * @param handle[in/out]  The handle for the module to be stopped
 *
 * @return 0 if successful, error otherwise
 */
int amod_stop(struct amod_handle *handle);

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param handle[in/out]   The handle for the receiving module instance
 * @param block[in]        Pointer to the audio data block to send to the module
 * @param response_cb[in]  Pointer to a callback to run when the buffer is
 *                         fully comsumed in a module
 *
 * @return 0 if successful, error otherwise
 */
int amod_data_tx(struct amod_handle *handle, struct ablk_block *block,
		 amod_response_cb response_cb);

/**
 * @brief Retrieve data from the module.
 *
 * @param handle[in/out]  The handle to the module instance
 * @param block[in]       Pointer to the audio data block from the module
 * @param timeout[in]     Non-negative waiting period to wait for operation to complete
 *	                      (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	                      or K_FOREVER to wait as long as necessary.
 *
 * @return 0 if successful, error otherwise
 */
int amod_data_rx(struct amod_handle *handle, struct ablk_block *block, k_timeout_t timeout);

/**
 * @brief Send an audio data block to a module and retrieve an audio data block from a module.
 *
 * @note The block is processed within the module or sequence of modules. The result is returned
 *       via the module or final module's output FIFO.
 *       All the input data is consumed within the call and thus the input data buffer
 *       maybe released once the function retrurns.
 *
 * @param handle_tx[in]  The handle to the module to send the input block to
 * @param handle_rx[in]  The handle to the module to receive a block from
 * @param block_tx[in]   Pointer to the audio data block to send
 * @param block_rx[out]  Pointer to the audio data block received
 * @param timeout[in]    Non-negative waiting period to wait for operation to complete
 *	                     (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	                     or K_FOREVER to wait as long as necessary.
 *
 * @return 0 if successful, error otherwise
 */
int amod_data_tx_rx(struct amod_handle *handle_tx, struct amod_handle *handle_rx,
		    struct ablk_block *block_tx, struct ablk_block *block_rx, k_timeout_t timeout);

/**
 * @brief Helper function to get the base and instance names for a given
 *        module handle.
 *
 * @param handle[in]          The handle to the module instance
 * @param base_name[out]      Pointer to the name of the module
 * @param instance_name[out]  Pointer to the name of the current module instance
 *
 * @return 0 if successful, error otherwise
 */
int amod_names_get(struct amod_handle *handle, char **base_name, char *instance_name);

/**
 * @brief Helper function to get the state of a given module handle.
 *
 * @param handle[in]  The handle to the module instance
 * @param state[out]  Pointer to the current module's state
 *
 * @return 0 if successful, error otherwise
 */
int amod_state_get(struct amod_handle *handle, enum amod_state *state);

/**
 * @brief Helper function to calculate the number of channels from the channel map for the given
 *        block.
 *
 * @param channel_map[in]       Channel mapping to calculate the number of channels from
 * @param number_channels[out]  Pointer to the calculated number of channels in the block
 *
 * @return 0 if successful, error otherwise
 */
int amod_number_channels_calculate(uint32_t channel_map, int8_t *number_channels);

#endif /*_AMOD_API_H_ */
