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
struct handle;

/**
 * @brief Private context for the module.
 */
struct amod_context;

/**
 * @brief Module's private configuration.
 */
struct amod_configuration;

/**
 * @brief Private pointer to a module's functions.
 */
struct amod_functions {
	/**
	 * @brief Function for opening a module.
	 *
	 * @param handle         The handle to the module instance
	 * @param configuration  Pointer to the module's current configuration
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*open)(struct handle *handle, struct amod_configuration *configuration);

	/**
	 * @brief Function to close an open module.
	 *
	 * @param handle  The handle to the module instance
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*close)(struct handle *handle);

	/**
	 * @brief Function to configure a module.
	 *
	 * @param handle         The handle to the module instance
	 * @param configuration  Pointer to the module's configuration to set
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*configuration_set)(struct handle *handle, struct amod_configuration *configuration);

	/**
	 * @brief Function to get the configuration of a module.
	 *
	 * @param handle         The handle to the module instance
	 * @param configuration  Pointer to the module's current configuration
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*configuration_get)(struct handle *handle, struct amod_configuration *configuration);

	/**
	 * @brief Start a module.
	 *
	 * @param handle  The handle for the module to start
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*start)(struct handle *handle);

	/**
	 * @brief Stop a module.
	 *
	 * @param handle  The handle for the module to stop
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*stop)(struct handle *handle);

	/**
	 * @brief The core data processing function for the module. Can be either an
	 *		  input, output or processor module type.
	 *
	 * @param handle    The handle to the module instance
	 * @param block_rx  Pointer to the input audio block or NULL for an input module
	 * @param block_tx  Pointer to the output audio block or NULL for an output module
	 *
	 * @return 0 if successful, error otherwise
	 */
	int (*data_process)(struct handle *handle, struct ablk_block *block_rx,
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

	/* A pointer to a module's data receiver FIFO */
	struct data_fifo *msg_rx;

	/* A pointer to a module's data transmitter FIFO */
	struct data_fifo *msg_tx;

	/* A pointer to the data buffer slab */
	struct k_mem_slab *data_slab;

	/* Size of each memory block in bytes that will be
	 * taken from the data buffer slab
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
 * @brief Callback function for a response to a data_send as
 *        supplied by the module user.
 *
 * @param handle  The handle of the module that sent the data message
 * @param block   The audio block to operate on
 */
typedef void (*amod_response_cb)(struct amod_handle *handle, struct ablk_block *block);

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
 * @brief A helper structure for describing the modules in a stream as a table.
 */
struct amod_table_entry {
	/* A unique name for the module */
	char *name;

	/* A unique handle for the module */
	struct amod_handle *handle;

	/* The initial configuration of the module */
	struct amod_configuration *initial_config;

	/* The module's parameters */
	struct amod_parameters params;

	/* A unique module context */
	struct amod_context *context;
};

/**
 * @brief Function for opening a module.
 *
 * @param parameters     Pointer to the module set-up parameters
 * @param configuration  Pointer to the module's configuration
 * @param name           A unique name for this instance of the module
 * @param handle         Pointer to the module's private handle
 * @param context        Pointer to the private context for the module
 *
 * @return 0 if successful, error otherwise
 */
int amod_open(struct amod_parameters *parameters, struct amod_configuration *configuration,
	      char *name, struct amod_handle *handle, struct amod_context *context);

/**
 * @brief Function to close an open module.
 *
 * @param handle  The handle to the module instance
 *
 * @return 0 if successful, error otherwise
 */
int amod_close(struct amod_handle *handle);

/**
 * @brief Function to reconfigure a module.
 *
 * @param handle         The handle to the module instance
 * @param configuration  Pointer to the module's configuration to set
 *
 * @return 0 if successful, error otherwise
 */
int amod_reconfigure(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief Function to get the configuration of a module.
 *
 * @param handle         The handle to the module instance
 * @param configuration  Pointer to the module's current configuration
 *
 * @return 0 if successful, error otherwise
 */
int amod_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration);

/**
 * @brief Function to connect two modules together.
 *
 * @param handle_from  The handle for the module for output
 * @param handle_to    The handle of the module for input. If it is the same as handle_from
 *                     the data will be put on the handle_from->output_message_quere.
 *
 * @return 0 if successful, error otherwise
 */
int amod_connect(struct amod_handle *handle_from, struct amod_handle *handle_to);

/**
 * @brief Function to disconnect modules from each other.
 *
 * @param handle             The handle for the module
 * @param handle_disconnect  The handle of the module to disconnect
 *
 * @return 0 if successful, error otherwise
 */
int amod_disconnect(struct amod_handle *handle, struct amod_handle *handle_disconnect);

/**
 * @brief Start processing data in the module given by handle.
 *
 * @param handle  The handle for the module to start
 *
 * @return 0 if successful, error otherwise
 */
int amod_start(struct amod_handle *handle);

/**
 * @brief Stop processing data in the module given by handle.
 *
 * @param handle  The handle for the module to be stopped
 *
 * @return 0 if successful, error otherwise
 */
int amod_stop(struct amod_handle *handle);

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param handle       The handle for the receiving module instance
 * @param block        Pointer to the audio data block to send to the module
 * @param response_cb  Pointer to a callback to run when the buffer is
 *                     fully comsumed in a module
 *
 * @return 0 if successful, error otherwise
 */
int amod_data_tx(struct amod_handle *handle, struct ablk_block *block,
		 amod_response_cb response_cb);

/**
 * @brief Retrieve data from the module.
 *
 * @param handle   The handle to the module instance
 * @param block    Pointer to the audio data block from the module
 * @param timeout  Non-negative waiting period to wait for operation to complete
 *	               (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	               or K_FOREVER to wait as long as necessary.
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
 * @param handle_tx  The handle to the module to send the input block to
 * @param handle_rx  The handle to the module to receive a block from
 * @param block_tx   Pointer to the audio data block to send
 * @param block_rx   Pointer to the audio data block received
 * @param timeout    Non-negative waiting period to wait for operation to complete
 *	                 (in milliseconds). Use K_NO_WAIT to return without waiting,
 *	                 or K_FOREVER to wait as long as necessary.
 *
 * @return 0 if successful, error otherwise
 */
int amod_data_tx_rx(struct amod_handle handle_tx, struct amod_handle handle_rx,
		    struct ablk_block *block_tx, struct ablk_block *block_rx, k_timeout_t timeout);

/**
 * @brief Helper function to configure the module's parameter structure with
 *        the module's description and thread information.
 *
 * @param parameters   Pointer to the module set-up parameters
 * @param description  Pointer to the module's private description
 * @param stack        Memory block for the threads stack
 * @param stack_size   Size of the threads stack
 * @param priority     Priority of the thread instance
 * @param msg_rx       Number of concurrent input messages
 * @param msg_tx       Number of concurrent output messages
 * @param data_slab    Slab to take a data item from
 * @param data_size    Size of the data item that will be taken
 *
 * @return 0 if successful, error otherwise
 */
int amod_parameters_configure(struct amod_parameters *parameters,
			      struct amod_description *description, k_thread_stack_t *stack,
			      size_t stack_size, int priority, struct data_fifo *msg_rx,
			      struct data_fifo *msg_tx, struct k_mem_slab *data_slab,
			      size_t data_size);

/**
 * @brief Helper function to get the base and instance names for a given
 *        module handle.
 *
 * @param handle         The handle to the module instance
 * @param base_name      Pointer to the name of the module
 * @param instance_name  Pointer to the name of the current module instance
 *
 * @return 0 if successful, error otherwise
 */
int amod_names_get(struct amod_handle *handle, char *base_name, char *instance_name);

/**
 * @brief Helper function to get the state of a given module handle.
 *
 * @param handle  The handle to the module instance
 * @param state   Pointer to the current module's state
 *
 * @return 0 if successful, error otherwise
 */
int amod_state_get(struct amod_handle *handle, enum amod_state *state);

/**
 * @brief Helper function to attach an audio data buffer to an audio block.
 *
 * @param block         Pointer to the audio data block to fill
 * @param data_type     The type of data carried in the block
 * @param data          Pointer to the PCM or coded audio data buffer
 * @param data_size     Size of the audio data buffer
 * @param format        The format od the data carried in the block
 * @param reference_ts  Reference timestamp to be used to synchronise the data
 * @param data_rx_ts    Data received timestamp to be used to synchronise the data
 * @param bad_frame     Flag to indicate there are errors within the data for this block
 * @param user_data     Pointer to a private area of user data or NULL
 *
 * @return 0 if successful, error otherwise
 */
int amod_block_data_attach(struct ablk_block *block, enum ablk_type data_type, char *data,
			   size_t data_size, struct ablk_pcm_format *format, uint32_t reference_ts,
			   uint32_t data_rx_ts, bool bad_frame, void *user_data);

/**
 * @brief Helper function to extract the audio data buffer from an audio block.
 *
 * @param block         Pointer to the audio data block to extract from
 * @param data_type     The type of data carried in the block
 * @param data          Pointer to the PCM or coded audio data buffer
 * @param data_size     Size of the audio data buffer
 * @param format        The format of the data carried in the block
 * @param reference_ts  Reference timestamp to be used to synchronise the data
 * @param data_rx_ts    Data received timestamp to be used to synchronise the data
 * @param bad_frame     Flag to indicate there are errors within the data for this block
 * @param user_data     Pointer to a private area of user data or NULL
 *
 * @return 0 if successful, error otherwise
 */
int amod_block_data_extract(struct ablk_block *block, enum ablk_type *data_type, char *data,
			    size_t *data_size, struct ablk_pcm_format *format,
			    uint32_t *reference_ts, uint32_t *data_rx_ts, bool *bad_frame,
			    void *user_data);

/**
 * @brief Helper function to calculate the number of channels from the channel map for the given
 *        block.
 *
 * @param block            Pointer to the audio data block
 * @param number_channels  Pointer to the calculated number of channels in the block
 *
 * @return 0 if successful, error otherwise
 */
int amod_number_channels_calculate(struct ablk_block *block, int8_t *number_channels);

#endif /*_AMOD_API_H_ */
