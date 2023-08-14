/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "amod_api.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "data_fifo.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(module_api, 4); /* CONFIG_MODULE_API_LOG_LEVEL);*/

/**
 * @brief Helper function to validate the module description.
 *
 * @param parameters  The module parameters
 *
 * @return 0 if successful, error value
 */
static int validate_parameters(struct amod_parameters *parameters)
{
	if (parameters == NULL) {
		LOG_DBG("No description for module");
		return -EINVAL;
	}

	if (parameters->description == NULL ||
	    (parameters->description->type != AMOD_TYPE_INPUT &&
	     parameters->description->type != AMOD_TYPE_OUTPUT &&
	     parameters->description->type != AMOD_TYPE_PROCESS) ||
	    parameters->description->name == NULL || parameters->description->functions == NULL) {
		LOG_DBG("Invalid description for module");
		return -EINVAL;
	}

	if (parameters->thread.stack == NULL || parameters->thread.stack_size == 0) {
		LOG_DBG("Invalid thread settings for module");
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief General callback for releasing the audio block when intermodule block
 *        passing.
 *
 * @param handle  The handle of the sending modules instance
 * @param block   Pointer to the audio data block to send to the module
 */
static void block_release_cb(struct amod_handle_private *handle, struct ablk_block *block)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;

	k_sem_take(&hdl->sem, K_NO_WAIT);

	if (k_sem_count_get(&hdl->sem) == 0) {
		/* Object data has been consumed by all modules so now can free the memory */
		k_mem_slab_free(hdl->thread.data_slab, (void **)&block->data);
	}
}

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param tx_handle    The handle for the sending module instance
 * @param rx_handle    The handle for the receiving module instance
 * @param block        Pointer to the audio data block to send to the module
 * @param response_cb  A pointer to a callback to run when the buffer is
 *                     fully comsumed
 *
 * @return 0 if successful, error value
 */
static int data_tx(struct amod_handle *tx_handle, struct amod_handle *rx_handle,
		   struct ablk_block *block, amod_response_cb data_in_response_cb)
{
	int ret;
	struct amod_message *data_msg_rx;

	if (rx_handle->state == AMOD_STATE_RUNNING) {
		ret = data_fifo_pointer_first_vacant_get(rx_handle->thread.msg_rx,
							 (void **)&data_msg_rx, K_NO_WAIT);
		if (ret) {
			LOG_DBG("Module %s no free data buffer, ret %d", rx_handle->name, ret);
			return ret;
		}

		/* fill data */
		memcpy(&data_msg_rx->block, block, sizeof(struct ablk_block));
		data_msg_rx->tx_handle = tx_handle;
		data_msg_rx->response_cb = data_in_response_cb;

		ret = data_fifo_block_lock(rx_handle->thread.msg_rx, (void **)&data_msg_rx,
					   sizeof(struct amod_message));
		if (ret) {
			data_fifo_block_free(rx_handle->thread.msg_rx, (void **)&data_msg_rx);

			LOG_DBG("Module %s failed to queue the block, ret %d", rx_handle->name,
				ret);
			return ret;
		}

		LOG_DBG("Block sent to module %s", rx_handle->name);

	} else {
		LOG_DBG("Recieving module %s is in an invalid state %d", rx_handle->name,
			rx_handle->state);
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Send to modules TX message queue.
 *
 * @param handle  The handle for this modules instance
 * @param block   A pointer to the audio block
 *
 * @return 0 if successful, error value
 */
static int send_to_tx_fifo(struct amod_handle *handle, struct ablk_block *block)
{
	int ret;
	struct amod_message *data_msg_tx;

	/* Take a TX message */
	ret = data_fifo_pointer_first_vacant_get(handle->thread.msg_tx, (void **)&data_msg_tx,
						 K_FOREVER);
	if (ret) {
		LOG_DBG("No free message for module %s, ret %d", handle->name, ret);
		return ret;
	}

	/* Configure audio block */
	memcpy(&data_msg_tx->block, block, sizeof(struct ablk_block));
	data_msg_tx->tx_handle = handle;
	data_msg_tx->response_cb = block_release_cb;

	/* Send audio block to modules output message queue */
	ret = data_fifo_block_lock(handle->thread.msg_tx, (void **)&data_msg_tx,
				   sizeof(struct amod_message));
	if (ret) {
		LOG_DBG("Failed to send block to output of module %s, ret %d", handle->name, ret);

		data_fifo_block_free(handle->thread.msg_tx, (void **)&data_msg_tx);

		k_sem_take(&handle->sem, K_NO_WAIT);

		return ret;
	}

	return 0;
}

/**
 * @brief Send output audio block to next module(s)
 *
 * @param handle  The handle for this modules instance
 * @param block   A pointer to the audio block
 *
 * @return 0 if successful, error value
 */
static int send_to_connected(struct amod_handle *handle, struct ablk_block *block)
{
	int ret;
	struct amod_handle *handle_to;

	if (handle->dest_count == 0) {
		LOG_DBG("No where to send the data from module %s so releasing it", handle->name);

		k_mem_slab_free(handle->thread.data_slab, (void **)block->data);

		return 0;
	}

	k_mutex_lock(&handle->dest_mutex, K_FOREVER);

	k_sem_init(&handle->sem, handle->dest_count, handle->dest_count);

	SYS_SLIST_FOR_EACH_CONTAINER(&handle->hdl_dest_list, handle_to, node) {
		ret = data_tx(handle, handle_to, block, &block_release_cb);
		if (ret) {
			LOG_DBG("Failed to send to module %s from %s, ret %d", handle_to->name,
				handle->name, ret);
			k_sem_take(&handle->sem, K_NO_WAIT);
		}
	}

	if (handle->data_tx && handle->thread.msg_tx) {
		ret = send_to_tx_fifo(handle, block);
		if (ret) {
			LOG_DBG("Failed to send block on module %s TX message queue", handle->name);
			k_sem_take(&handle->sem, K_NO_WAIT);
		}
	}

	k_mutex_unlock(&handle->dest_mutex);

	return 0;
}

/**
 * @brief The thread that receives data from outside the system and passes it into the audio system.
 *
 * @param handle  The handle for this modules instance
 *
 * @return 0 if successful, error value
 */
static int module_thread_input(struct amod_handle *handle)
{
	int ret;
	struct ablk_block block;
	void *data;

	if (handle == NULL) {
		LOG_DBG("Module task has NULL handle");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		data = NULL;

		if (handle->description->functions->data_process != NULL) {
			/* Get a new output buffer */
			ret = k_mem_slab_alloc(handle->thread.data_slab, (void **)&data, K_FOREVER);
			if (ret) {
				LOG_DBG("No free data for module %s, ret %d", handle->name, ret);
				continue;
			}

			/* Configure new audio block */
			block.data = data;
			block.data_size = handle->thread.data_size;

			/* Process the input audio block */
			ret = handle->description->functions->data_process(
				(struct amod_handle_private *)handle, NULL, &block);
			if (ret) {
				k_mem_slab_free(handle->thread.data_slab, (void **)(&data));

				LOG_DBG("Data process error in module %s, ret %d", handle->name,
					ret);
				continue;
			}

			/* Send input audio block to next module(s) */
			send_to_connected(handle, &block);
		} else {
			LOG_DBG("No process function for module %s, no input", handle->name);
		}
	}

	return 0;
}

/**
 * @brief The thread that processes inputs and outputs them out of the audio system.
 *
 * @param handle  The handle for this modules instance
 *
 * @return 0 if successful, error value
 */
static int module_thread_output(struct amod_handle *handle)
{
	int ret;

	struct amod_message *msg_rx;
	size_t size;

	if (handle == NULL) {
		LOG_DBG("Module task has NULL handle");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		msg_rx = NULL;

		ret = data_fifo_pointer_last_filled_get(handle->thread.msg_rx, (void **)&msg_rx,
							&size, K_FOREVER);
		if (ret) {
			LOG_DBG("No new data for module %s, ret %d", handle->name, ret);
			continue;
		}

		if (handle->description->functions->data_process != NULL) {
			/* Process the input audio block and output from the audio system */
			ret = handle->description->functions->data_process(
				(struct amod_handle_private *)handle, &msg_rx->block, NULL);
			if (ret) {
				if (msg_rx->response_cb != NULL) {
					msg_rx->response_cb(
						(struct amod_handle_private *)msg_rx->tx_handle,
						&msg_rx->block);
				}

				LOG_DBG("Data process error in module %s, ret %d", handle->name,
					ret);
				continue;
			}

			if (msg_rx->response_cb != NULL) {
				msg_rx->response_cb((struct amod_handle_private *)msg_rx->tx_handle,
						    &msg_rx->block);
			}
		} else {
			LOG_DBG("No process function for module %s, discarding input",
				handle->name);
		}

		data_fifo_block_free(handle->thread.msg_rx, (void **)&msg_rx);
	}

	return 0;
}

/**
 * @brief The thread that processes inputs and outputs the data from the module
 *
 * @param handle  The handle for this modules instance
 *
 * @return 0 if successful, error value
 */
static int module_thread_in_out(struct amod_handle *handle)
{
	int ret;
	struct amod_message *msg_rx;
	struct ablk_block block;
	void *data;
	size_t size;

	if (handle == NULL) {
		LOG_DBG("Module task has NULL handle");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		data = NULL;

		ret = data_fifo_pointer_last_filled_get(handle->thread.msg_rx, (void **)&msg_rx,
							&size, K_FOREVER);
		if (ret) {
			LOG_DBG("No new message for module %s, ret %d", handle->name, ret);
			continue;
		}

		if (handle->description->functions->data_process != NULL) {
			/* Get a new output buffer from outside the audio system */
			ret = k_mem_slab_alloc(handle->thread.data_slab, (void **)&data, K_NO_WAIT);
			if (ret) {
				if (msg_rx->response_cb != NULL) {
					msg_rx->response_cb(
						(struct amod_handle_private *)(msg_rx->tx_handle),
						&msg_rx->block);
				}

				data_fifo_block_free(handle->thread.msg_rx, (void **)(&msg_rx));

				LOG_DBG("No free data buffer for module %s, dropping input, ret %d",
					handle->name, ret);
				continue;
			}

			/* Configure new audio block */
			block.data = data;
			block.data_size = handle->thread.data_size;

			/* Process the input audio block into the output audio block */
			ret = handle->description->functions->data_process(
				(struct amod_handle_private *)handle, &msg_rx->block, &block);
			if (ret) {
				if (msg_rx->response_cb != NULL) {
					msg_rx->response_cb(
						(struct amod_handle_private *)(msg_rx->tx_handle),
						&msg_rx->block);
				}

				data_fifo_block_free(handle->thread.msg_rx, (void **)(&msg_rx));

				k_mem_slab_free(handle->thread.data_slab, (void **)(&data));

				LOG_DBG("Data process error in module %s, ret %d", handle->name,
					ret);
				continue;
			}

			/* Send input audio block to next module(s) */
			send_to_connected(handle, &block);

			if (msg_rx->response_cb != NULL) {
				msg_rx->response_cb((struct amod_handle_private *)msg_rx->tx_handle,
						    &msg_rx->block);
			}
		} else {
			LOG_DBG("No process function for module %s, discarding input",
				handle->name);
		}

		data_fifo_block_free(handle->thread.msg_rx, (void **)&msg_rx);
	}

	return 0;
}

/**
 * @brief  Function for opening a module.
 */
int amod_open(struct amod_parameters *parameters, struct amod_configuration *configuration,
	      char *name, struct amod_context *context, struct amod_handle *handle)
{
	int ret;

	if (parameters == NULL || configuration == NULL || name == NULL || handle == NULL ||
	    context == NULL) {
		LOG_DBG("Parameter is NULL for module open function");
		return -EINVAL;
	}

	if (handle->state != AMOD_STATE_UNDEFINED) {
		LOG_DBG("The module is already open");
		return -EALREADY;
	}

	ret = validate_parameters(parameters);
	if (ret) {
		LOG_DBG("Invalid parameters for module, ret %d", ret);
		return ret;
	}

	memset(handle, 0, sizeof(struct amod_handle));

	handle->description->type = parameters->description->type;
	handle->previous_state = AMOD_STATE_UNDEFINED;

	/* Allocate the context memory */
	handle->context = context;

	memcpy(handle->name, name, CONFIG_AMOD_NAME_SIZE);
	if (strlen(name) > CONFIG_AMOD_NAME_SIZE) {
		LOG_INF("Module's instance name truncated to %s", handle->name);
	}

	handle->description = parameters->description;
	memcpy(&handle->thread, &parameters->thread, sizeof(struct amod_thread_configuration));

	if (handle->description->functions->open != NULL) {
		ret = handle->description->functions->open((struct amod_handle_private *)handle,
							   configuration);
		if (ret) {
			LOG_DBG("Failed open call to module %s, ret %d", name, ret);
			return ret;
		}
	}

	if (handle->description->functions->configuration_set != NULL) {
		ret = handle->description->functions->configuration_set(
			(struct amod_handle_private *)handle, configuration);
		if (ret) {
			LOG_DBG("Set configuration for module %s send failed, ret %d", handle->name,
				ret);
			return ret;
		}
	}

	switch (handle->description->type) {
	case AMOD_TYPE_INPUT:
		handle->thread_id = k_thread_create(
			&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
			(k_thread_entry_t)module_thread_input, handle, NULL, NULL,
			K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

		ret = k_thread_name_set(handle->thread_id, handle->name);
		if (ret) {
			LOG_DBG("Failed to start input module %s thread, ret %d", handle->name,
				ret);
		}

		break;

	case AMOD_TYPE_OUTPUT:
		handle->thread_id = k_thread_create(
			&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
			(k_thread_entry_t)module_thread_output, handle, NULL, NULL,
			K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

		ret = k_thread_name_set(handle->thread_id, handle->name);
		if (ret) {
			LOG_DBG("Failed to start output module %s thread, ret %d", handle->name,
				ret);
		}

		break;

	case AMOD_TYPE_PROCESS:
		handle->thread_id = k_thread_create(
			&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
			(k_thread_entry_t)module_thread_in_out, handle, NULL, NULL,
			K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

		ret = k_thread_name_set(handle->thread_id, handle->name);
		if (ret) {
			LOG_DBG("Failed to start processor module %s thread, ret %d", handle->name,
				ret);
		}

		break;

	default:
		LOG_DBG("Invalid module type %d for module %s", handle->description->type,
			handle->name);
		ret = -EINVAL;
	}

	if (ret) {
		/* Clean up the handle */
		memset(handle, 0, sizeof(struct amod_handle));
		return ret;
	}

	sys_slist_init(&handle->hdl_dest_list);
	k_mutex_init(&handle->dest_mutex);

	k_thread_start(handle->thread_id);

	handle->state = AMOD_STATE_CONFIGURED;

	LOG_DBG("Module %s is now open", handle->name);

	return 0;
}

/**
 * @brief  Function to close an open module.
 */
int amod_close(struct amod_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_UNDEFINED || handle->state == AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s in an invalid state, %d, for close", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->close != NULL) {
		ret = handle->description->functions->close((struct amod_handle_private *)handle);
		if (ret) {
			LOG_DBG("Failed close call to module %s, returned %d", handle->name, ret);
			return ret;
		}
	}

	if (handle->thread.msg_rx != NULL) {
		data_fifo_empty(handle->thread.msg_rx);
	}

	if (handle->thread.msg_tx != NULL) {
		data_fifo_empty(handle->thread.msg_tx);
	}

	/*
	 * How to return all the data to the slab items?
	 * Test the semephore and wait for it to be zero.
	 */

	k_thread_abort(handle->thread_id);

	/* Clean up the handle */
	memset(handle, 0, sizeof(struct amod_handle));

	return 0;
};

/**
 * @brief  Function to reconfigure a module.
 */
int amod_reconfigure(struct amod_handle *handle, struct amod_configuration *configuration)
{
	int ret;

	if (handle == NULL || configuration == NULL) {
		LOG_DBG("Module input parameter error");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_UNDEFINED || handle->state == AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s in an invalid state, %d, for setting the configuration",
			handle->name, handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->configuration_set != NULL) {
		ret = handle->description->functions->configuration_set(
			(struct amod_handle_private *)handle, configuration);
		if (ret) {
			LOG_DBG("Set configuration for module %s send failed, ret %d", handle->name,
				ret);
			return ret;
		}
	} else {
		LOG_DBG("Reconfiguration for module %s has no set configuration function",
			handle->name);
	}

	if (handle->state != AMOD_STATE_CONFIGURED) {
		handle->previous_state = handle->state;
	}

	handle->state = AMOD_STATE_CONFIGURED;

	return 0;
};

/**
 * @brief  Function to get the configuration of a module.
 */
int amod_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration)
{
	int ret;

	if (handle == NULL || configuration == NULL) {
		LOG_DBG("Module input parameter error");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_UNDEFINED) {
		LOG_DBG("Module %s in an invalid state, %d, for getting the configuration",
			handle->name, handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->configuration_get != NULL) {
		ret = handle->description->functions->configuration_get(
			(struct amod_handle_private *)handle, configuration);
		if (ret) {
			LOG_DBG("Get configuration for module %s failed, ret %d", handle->name,
				ret);
			return ret;
		}
	} else {
		LOG_DBG("Get configuration for module %s has no get configuration function",
			handle->name);
	}

	return 0;
};

/**
 * @brief Function to connect two modules together.
 *
 */
int amod_connect(struct amod_handle *handle_from, struct amod_handle *handle_to)
{
	struct amod_handle *handle;

	if (handle_from == NULL || handle_to == NULL) {
		LOG_DBG("Invalid parameter for the connection function");
		return -EINVAL;
	}

	if (handle_from->description->type == AMOD_TYPE_UNDEFINED ||
	    handle_from->description->type == AMOD_TYPE_OUTPUT ||
	    handle_to->description->type == AMOD_TYPE_UNDEFINED ||
	    handle_to->description->type == AMOD_TYPE_INPUT) {
		LOG_DBG("Connections between these modules, %s to %s, is not supported",
			handle_from->name, handle_to->name);
		return -ENOTSUP;
	}

	if (handle_from->state == AMOD_STATE_UNDEFINED ||
	    handle_to->state == AMOD_STATE_UNDEFINED) {
		LOG_DBG("A module is in an invalid state for connecting modules %s to %s",
			handle_from->name, handle_to->name);
		return -ENOTSUP;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&handle_from->hdl_dest_list, handle, node) {
		if (handle_to == handle) {
			LOG_DBG("Already attached %s to %s", handle_to->name, handle_from->name);
			return -EALREADY;
		}
	}

	k_mutex_lock(&handle_from->dest_mutex, K_FOREVER);

	if (handle_from == handle_to) {
		handle_from->data_tx = true;
		handle_from->dest_count += 1;
	} else {
		sys_slist_append(&handle_from->hdl_dest_list, &handle_to->node);
		handle_from->dest_count += 1;
	}

	k_mutex_unlock(&handle_from->dest_mutex);

	LOG_DBG("Connected the output of %s to the input of %s", handle_from->name,
		handle_to->name);

	return 0;
}

/**
 * @brief Function to disconnect modules from each other.
 *
 */
int amod_disconnect(struct amod_handle *handle, struct amod_handle *handle_disconnect)
{
	if (handle == NULL || handle_disconnect == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->description->type == AMOD_TYPE_UNDEFINED ||
	    handle->description->type == AMOD_TYPE_OUTPUT ||
	    handle_disconnect->description->type == AMOD_TYPE_UNDEFINED ||
	    handle_disconnect->description->type == AMOD_TYPE_INPUT) {
		LOG_DBG("Disonnection of modules %s from %s, is not supported",
			handle_disconnect->name, handle->name);
		return -ENOTSUP;
	}

	if (handle->state == AMOD_STATE_UNDEFINED ||
	    handle_disconnect->state == AMOD_STATE_UNDEFINED) {
		LOG_DBG("A module is an invalid state or type for disconnection");
		return -ENOTSUP;
	}

	k_mutex_lock(&handle->dest_mutex, K_FOREVER);

	if (handle == handle_disconnect) {
		handle->data_tx = false;
		handle->dest_count -= 1;
	} else {
		if (!sys_slist_find_and_remove(&handle->hdl_dest_list, &handle_disconnect->node)) {
			LOG_DBG("Connection to module %s has not been found for module %s",
				handle_disconnect->name, handle->name);
			return -EALREADY;
		} else {
			LOG_DBG("Disconnect module %s from module %s", handle_disconnect->name,
				handle->name);
			handle->dest_count -= 1;
		}
	}

	k_mutex_unlock(&handle->dest_mutex);

	return 0;
}

/**
 * @brief Start processing data in the module given by handle.
 */
int amod_start(struct amod_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_UNDEFINED) {
		LOG_DBG("Module %s in an invalid state, %d, for start", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	if (handle->state == AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s already running", handle->name);
		return -EALREADY;
	}

	if (handle->description->functions->start != NULL) {
		ret = handle->description->functions->start((struct amod_handle_private *)handle);
		if (ret < 0) {
			LOG_DBG("Failed user start for module %s, ret %d", handle->name, ret);
			return ret;
		}
	}

	handle->previous_state = handle->state;
	handle->state = AMOD_STATE_RUNNING;

	return 0;
}

/**
 * @brief Stop processing data in the module given by handle.
 */
int amod_stop(struct amod_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_STOPPED) {
		LOG_DBG("Module %s already stopped", handle->name);
		return -EALREADY;
	}

	if (handle->state != AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s in an invalid state, %d, for stop", handle->name, handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->stop != NULL) {
		ret = handle->description->functions->stop((struct amod_handle_private *)handle);
		if (ret < 0) {
			LOG_DBG("Failed user pause for module %s, ret %d", handle->name, ret);
			return ret;
		}
	}

	handle->previous_state = handle->state;
	handle->state = AMOD_STATE_STOPPED;

	return 0;
}

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 */
int amod_data_tx(struct amod_handle *handle, struct ablk_block *block, amod_response_cb response_cb)
{
	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (block == NULL || block->data == NULL || block->data_size == 0) {
		LOG_DBG("Module , %s, data parameter error", handle->name);
		return -ECONNREFUSED;
	}

	if (handle->state != AMOD_STATE_RUNNING ||
	    handle->description->type == AMOD_TYPE_UNDEFINED ||
	    handle->description->type == AMOD_TYPE_INPUT) {
		LOG_DBG("Module %s in an invalid state (%d) or type (%d) to transmit data",
			handle->name, handle->state, handle->description->type);
		return -ENOTSUP;
	}

	return data_tx((void *)NULL, handle, block, response_cb);
}

/**
 * @brief Retrieve data from the module.
 *
 */
int amod_data_rx(struct amod_handle *handle, struct ablk_block *block, k_timeout_t timeout)
{
	int ret;

	struct amod_message *msg_tx;
	size_t msg_tx_size;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state != AMOD_STATE_RUNNING ||
	    handle->description->type == AMOD_TYPE_UNDEFINED ||
	    handle->description->type == AMOD_TYPE_OUTPUT) {
		LOG_DBG("Module %s in an invalid state (%d) or type (%d) to receive data",
			handle->name, handle->state, handle->description->type);
		return -ENOTSUP;
	}

	if (block == NULL || block->data == NULL || block->data_size == 0) {
		LOG_DBG("Error in audio block for module %s", handle->name);
		return -ECONNREFUSED;
	}

	ret = data_fifo_pointer_last_filled_get(handle->thread.msg_tx, (void **)&msg_tx,
						&msg_tx_size, timeout);
	if (ret) {
		LOG_DBG("Failed to retrieve data from module %s, ret %d", handle->name, ret);
		return ret;
	}

	if (msg_tx->block.data == NULL || msg_tx->block.data_size > block->data_size) {
		LOG_DBG("Data output buffer too small for received buffer from module %s",
			handle->name);
		ret = -EINVAL;
	} else {
		void *data_out = block->data;

		memcpy(block, &msg_tx->block, sizeof(struct ablk_block));
		memcpy((uint8_t *)data_out, (uint8_t *)msg_tx->block.data, msg_tx->block.data_size);
	}

	block_release_cb((struct amod_handle_private *)handle, &msg_tx->block);

	data_fifo_block_free(handle->thread.msg_tx, (void **)&msg_tx);

	return ret;
}

/**
 * @brief Send an audio data block to a module and retrieve an audio data block from a module.
 *
 * @note The block is processed within the module or sequence of modules. The result is returned
 *       via the module or final module's output FIFO.
 *       All the input data is consumed within the call and thus the input data buffer
 *       maybe released once the function retrurns.
 *
 */
int amod_data_tx_rx(struct amod_handle *handle_tx, struct amod_handle *handle_rx,
		    struct ablk_block *block_tx, struct ablk_block *block_rx, k_timeout_t timeout)
{
	int ret;
	struct amod_message *msg_tx;
	size_t msg_tx_size;

	if (handle_tx == NULL || handle_rx == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle_tx->state != AMOD_STATE_RUNNING || handle_rx->state != AMOD_STATE_RUNNING) {
		LOG_DBG("Module is in an invalid state or type to send-receive data");
		return -ENOTSUP;
	}

	if (handle_tx->description->type == AMOD_TYPE_UNDEFINED ||
	    handle_tx->description->type == AMOD_TYPE_INPUT ||
	    handle_rx->description->type == AMOD_TYPE_UNDEFINED ||
	    handle_rx->description->type == AMOD_TYPE_OUTPUT) {
		LOG_DBG("Module not of the right type for operation");
		return -ENOTSUP;
	}

	if (block_tx == NULL || block_rx == NULL) {
		LOG_DBG("Block pointer for module is NULL");
		return -ECONNREFUSED;
	}

	if (block_tx->data == NULL || block_tx->data_size == 0 || block_rx->data == NULL ||
	    block_rx->data_size == 0) {
		LOG_DBG("Invalid output audio block");
		return -ECONNREFUSED;
	}

	if (handle_tx->thread.msg_rx == NULL || handle_rx->thread.msg_tx == NULL) {
		LOG_DBG("Modules have message queue set to NULL");
		return -ENOTSUP;
	}

	ret = data_tx(NULL, handle_tx, block_tx, NULL);
	if (ret) {
		LOG_DBG("Failed to send data to module %s, ret %d", handle_tx->name, ret);
		return ret;
	}

	ret = data_fifo_pointer_last_filled_get(handle_rx->thread.msg_tx, (void **)&msg_tx,
						&msg_tx_size, timeout);
	if (ret) {
		LOG_DBG("Failed to retrieve data from module %s, ret %d", handle_rx->name, ret);
		return ret;
	}

	if (msg_tx->block.data == NULL || msg_tx->block.data_size == 0) {
		LOG_DBG("Data output buffer too small for received buffer from module %s (%d)",
			handle_rx->name, msg_tx->block.data_size);
		ret = -EINVAL;
	} else {
		memcpy(block_rx, &msg_tx->block, sizeof(struct ablk_block));
		memcpy((uint8_t *)block_rx->data, (uint8_t *)msg_tx->block.data,
		       msg_tx->block.data_size);
		block_rx->data_size = msg_tx->block.data_size;
	}

	block_release_cb((struct amod_handle_private *)handle_rx, &msg_tx->block);

	data_fifo_block_free(handle_rx->thread.msg_tx, (void **)&msg_tx);

	return ret;
};

/**
 * @brief Helper function to return the base and instance names for a given
 *        module handle.
 *
 */
int amod_names_get(struct amod_handle *handle, char **base_name, char *instance_name)
{
	if (handle == NULL || base_name == NULL || instance_name == NULL) {
		LOG_DBG("Input parameter is NULL");
		return -EINVAL;
	}

	if (handle->state == AMOD_STATE_UNDEFINED) {
		LOG_DBG("Module %s is in an invalid state, %d, for get names", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	*base_name = handle->description->name;
	memcpy(instance_name, &handle->name, CONFIG_AMOD_NAME_SIZE);

	return 0;
}

/**
 * @brief Helper function to get the state of a given module handle.
 *
 */
int amod_state_get(struct amod_handle *handle, enum amod_state *state)
{
	if (handle == NULL || state == NULL) {
		LOG_DBG("Input parameter is NULL");
		return -EINVAL;
	}

	if (handle->state > AMOD_STATE_STOPPED) {
		LOG_DBG("Module state is invalid");
		return -EINVAL;
	}

	*state = handle->state;

	return 0;
};

/**
 * @brief Calculate the number of channels from the channel map for the given module handle.
 *
 */
int amod_number_channels_calculate(uint32_t channel_map, int8_t *number_channels)
{
	if (number_channels == NULL) {
		LOG_DBG("Invalid parameters");
		return -EINVAL;
	}

	*number_channels = 0;
	for (int i = 0; i < AMOD_BITS_IN_CHANNEL_MAP; i++) {
		*number_channels += channel_map & 1;
		channel_map >>= 1;
	}

	return 0;
}
