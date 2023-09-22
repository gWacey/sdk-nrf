/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_module.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "data_fifo.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_module, CONFIG_AUDIO_MODULE_LOG_LEVEL);

/* Define a timeout to prevent system locking */
#define LOCK_TIMEOUT_US (K_USEC(100))

/**
 * @brief Helper function to validate the module description.
 *
 * @param parameters  The module parameters.
 *
 * @return 0 if successful, error value.
 */
static int validate_parameters(struct audio_module_parameters *parameters)
{
	if (parameters == NULL) {
		LOG_ERR("No parameters for module");
		return -EINVAL;
	}

	if (parameters->description == NULL ||
	    (parameters->description->type != AUDIO_MODULE_TYPE_INPUT &&
	     parameters->description->type != AUDIO_MODULE_TYPE_OUTPUT &&
	     parameters->description->type != AUDIO_MODULE_TYPE_IN_OUT) ||
	    parameters->description->name == NULL || parameters->description->functions == NULL) {
		return -EINVAL;
	}

	if (parameters->description->functions->configuration_set == NULL ||
	    parameters->description->functions->data_process == NULL) {
		return -EINVAL;
	}

	if (parameters->thread.stack == NULL || parameters->thread.stack_size == 0) {
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Helper function to validate the module types for connecting and disconnecting.
 *
 * @param handle_from  The from/sending handle.
 * @param handle_to    The to/receiving handle.
 *
 * @return 0 if successful, error value.
 */
static int handle_connection_type_test(struct audio_module_handle *handle_from,
				       struct audio_module_handle *handle_to)
{
	if (handle_from->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle_from->description->type == AUDIO_MODULE_TYPE_OUTPUT ||
	    handle_to->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle_to->description->type == AUDIO_MODULE_TYPE_INPUT) {
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief General callback for releasing the audio data when inter-module data
 *        passing.
 *
 * @param handle      The handle of the sending modules instance.
 * @param audio_data  Pointer to the audio data to send to the module.
 */
static void audio_data_release_cb(struct audio_module_handle_private *handle,
				  struct audio_data *audio_data)
{
	int ret;
	struct audio_module_handle *hdl = (struct audio_module_handle *)handle;

	ret = k_sem_take(&hdl->sem, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to take semaphore for data release callback function");
	}

	if (k_sem_count_get(&hdl->sem) == 0) {
		/* Object data has been consumed by all modules so now can free the memory. */
		k_mem_slab_free(hdl->thread.data_slab, (void **)&audio_data->data);
	}
}

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param tx_handle    The handle for the sending module instance.
 * @param rx_handle    The handle for the receiving module instance.
 * @param audio_data   Pointer to the audio data  to send to the module.
 * @param response_cb  A pointer to a callback to run when the buffer is
 *                     fully consumed.
 *
 * @return 0 if successful, error value.
 */
static int data_tx(struct audio_module_handle *tx_handle, struct audio_module_handle *rx_handle,
		   struct audio_data *audio_data, audio_module_response_cb data_in_response_cb)
{
	int ret;
	struct audio_module_message *data_msg_rx;

	if (rx_handle->state == AUDIO_MODULE_STATE_RUNNING) {
		ret = data_fifo_pointer_first_vacant_get(rx_handle->thread.msg_rx,
							 (void **)&data_msg_rx, K_NO_WAIT);
		if (ret) {
			LOG_DBG("Module %s no free data buffer, ret %d", rx_handle->name, ret);
			return ret;
		}

		/* Copy. The audio data itself will remain in its original location. */
		memcpy(&data_msg_rx->audio_data, audio_data, sizeof(struct audio_data));
		data_msg_rx->tx_handle = tx_handle;
		data_msg_rx->response_cb = data_in_response_cb;

		ret = data_fifo_block_lock(rx_handle->thread.msg_rx, (void **)&data_msg_rx,
					   sizeof(struct audio_module_message));
		if (ret) {
			data_fifo_block_free(rx_handle->thread.msg_rx, (void **)&data_msg_rx);

			LOG_WRN("Module %s failed to queue audio data, ret %d", rx_handle->name,
				ret);
			return ret;
		}

		LOG_DBG("Block sent to module %s", rx_handle->name);

	} else {
		LOG_DBG("Receiving module %s is in an invalid state %d", rx_handle->name,
			rx_handle->state);
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Send to modules TX message queue.
 *
 * @param handle      The handle for this modules instance.
 * @param audio_data  A pointer to the audio data.
 *
 * @return 0 if successful, error value.
 */
static int tx_fifo_put(struct audio_module_handle *handle, struct audio_data *audio_data)
{
	int ret;
	struct audio_module_message *data_msg_tx;

	/* Take a TX message. */
	ret = data_fifo_pointer_first_vacant_get(handle->thread.msg_tx, (void **)&data_msg_tx,
						 K_NO_WAIT);
	if (ret) {
		LOG_DBG("No free space in TX FIFO for module %s, ret %d", handle->name, ret);
		return ret;
	}

	/* Configure audio data. */
	memcpy(&data_msg_tx->audio_data, audio_data, sizeof(struct audio_data));
	data_msg_tx->tx_handle = handle;
	data_msg_tx->response_cb = audio_data_release_cb;

	/* Send audio data to modules output message queue. */
	ret = data_fifo_block_lock(handle->thread.msg_tx, (void **)&data_msg_tx,
				   sizeof(struct audio_module_message));
	if (ret) {
		LOG_ERR("Failed to send audio data to output of module %s, ret %d", handle->name,
			ret);

		data_fifo_block_free(handle->thread.msg_tx, (void **)&data_msg_tx);

		k_sem_take(&handle->sem, K_NO_WAIT);

		return ret;
	}

	return 0;
}

/**
 * @brief Send output audio data to next module(s).
 *
 * @param handle      The handle for this modules instance.
 * @param audio_data  A pointer to the audio data.
 *
 * @return 0 if successful, error value.
 */
static int send_to_connected_modules(struct audio_module_handle *handle,
				     struct audio_data *audio_data)
{
	int ret;
	struct audio_module_handle *handle_to;

	if (handle->dest_count == 0) {
		LOG_WRN("Nowhere to send the data from module %s so releasing it", handle->name);

		k_mem_slab_free(handle->thread.data_slab, (void **)audio_data->data);

		return 0;
	}

	/* We need to ensure that all receiving modules have got the audio.
	 * This is so the first receiver cannot free the data before all receivers
	 * have gotten the data.
	 */
	ret = k_mutex_lock(&handle->dest_mutex, LOCK_TIMEOUT_US);
	if (ret) {
		return ret;
	}

	k_sem_init(&handle->sem, handle->dest_count, handle->dest_count);

	SYS_SLIST_FOR_EACH_CONTAINER(&handle->hdl_dest_list, handle_to, node) {
		ret = data_tx(handle, handle_to, audio_data, &audio_data_release_cb);
		if (ret) {
			LOG_ERR("Failed to send audio data to module %s from %s, ret %d",
				handle_to->name, handle->name, ret);
			k_sem_take(&handle->sem, K_NO_WAIT);
		}
	}

	k_mutex_unlock(&handle->dest_mutex);

	if (handle->data_tx && handle->thread.msg_tx) {
		ret = tx_fifo_put(handle, audio_data);
		if (ret) {
			LOG_ERR("Failed to send audio data on module %s TX message queue",
				handle->name);
			k_sem_take(&handle->sem, K_NO_WAIT);
		}
	}

	return 0;
}

/**
 * @brief The thread that receives data from outside the system and passes it into the audio system.
 *
 * @param handle  The handle for this modules instance.
 *
 * @return 0 if successful, error value.
 */
static void module_thread_input(struct audio_module_handle *handle, void *p2, void *p3)
{
	int ret;
	struct audio_data audio_data;
	void *data;

	__ASSERT(handle != NULL, "Module task has NULL handle");
	__ASSERT(handle->description->functions->data_process != NULL,
		 "Module task has NULL process function pointer");

	/* Execute thread */
	while (1) {
		data = NULL;

		/* Get a new output buffer.
		 * Since this input module generates data within itself, the module itself
		 * will control the data flow.
		 */
		ret = k_mem_slab_alloc(handle->thread.data_slab, (void **)&data, K_NO_WAIT);
		if (ret) {
			LOG_ERR("No free data for module %s, ret %d", handle->name, ret);
			continue;
		}

		/* Configure new audio data. */
		audio_data.data = data;
		audio_data.data_size = handle->thread.data_size;

		/* Process the input audio data */
		ret = handle->description->functions->data_process(
			(struct audio_module_handle_private *)handle, NULL, &audio_data);
		if (ret) {
			k_mem_slab_free(handle->thread.data_slab, (void **)(&data));

			LOG_ERR("Data process error in module %s, ret %d", handle->name, ret);
			continue;
		}

		/* Send input audio data to next module(s). */
		send_to_connected_modules(handle, &audio_data);
	}

	CODE_UNREACHABLE;
}

/**
 * @brief The thread that processes inputs and outputs them out of the audio system.
 *
 * @param handle  The handle for this modules instance.
 *
 * @return 0 if successful, error value.
 */
static void module_thread_output(struct audio_module_handle *handle, void *p2, void *p3)
{
	int ret;

	struct audio_module_message *msg_rx;
	size_t size;

	__ASSERT(handle != NULL, "Module task has NULL handle");
	__ASSERT(handle->description->functions->data_process != NULL,
		 "Module task has NULL process function pointer");

	/* Execute thread. */
	while (1) {
		msg_rx = NULL;

		ret = data_fifo_pointer_last_filled_get(handle->thread.msg_rx, (void **)&msg_rx,
							&size, K_FOREVER);
		if (ret) {
			LOG_ERR("No new data for module %s, ret %d", handle->name, ret);
		}

		/* Process the input audio data and output from the audio system. */
		ret = handle->description->functions->data_process(
			(struct audio_module_handle_private *)handle, &msg_rx->audio_data, NULL);
		if (ret) {
			if (msg_rx->response_cb != NULL) {
				msg_rx->response_cb(
					(struct audio_module_handle_private *)msg_rx->tx_handle,
					&msg_rx->audio_data);
			}

			LOG_ERR("Data process error in module %s, ret %d", handle->name, ret);
			continue;
		}

		if (msg_rx->response_cb != NULL) {
			msg_rx->response_cb((struct audio_module_handle_private *)msg_rx->tx_handle,
					    &msg_rx->audio_data);
		}

		data_fifo_block_free(handle->thread.msg_rx, (void **)&msg_rx);
	}

	CODE_UNREACHABLE;
}

/**
 * @brief The thread that processes inputs and outputs the data from the module.
 *
 * @param handle  The handle for this modules instance.
 *
 * @return 0 if successful, error value.
 */
static void module_thread_in_out(struct audio_module_handle *handle, void *p2, void *p3)
{
	int ret;
	struct audio_module_message *msg_rx;
	struct audio_data audio_data;
	void *data;
	size_t size;

	__ASSERT(handle != NULL, "Module task has NULL handle");
	__ASSERT(handle->description->functions->data_process != NULL,
		 "Module task has NULL process function pointer");

	/* Execute thread. */
	while (1) {
		data = NULL;

		LOG_DBG("Module %s is waiting for audio data", handle->name);

		ret = data_fifo_pointer_last_filled_get(handle->thread.msg_rx, (void **)&msg_rx,
							&size, K_FOREVER);
		if (ret) {
			LOG_ERR("No new message for module %s, ret %d", handle->name, ret);
		}

		LOG_DBG("Module %s new message received", handle->name);

		/* Get a new output buffer from outside the audio system. */
		ret = k_mem_slab_alloc(handle->thread.data_slab, (void **)&data, K_NO_WAIT);
		if (ret) {
			if (msg_rx->response_cb != NULL) {
				msg_rx->response_cb(
					(struct audio_module_handle_private *)(msg_rx->tx_handle),
					&msg_rx->audio_data);
			}

			data_fifo_block_free(handle->thread.msg_rx, (void **)(&msg_rx));

			LOG_DBG("No free data buffer for module %s, dropping input, ret %d",
				handle->name, ret);
			continue;
		}

		/* Configure new audio audio_data. */
		audio_data.data = data;
		audio_data.data_size = handle->thread.data_size;

		/* Process the input audio data into the output audio data. */
		ret = handle->description->functions->data_process(
			(struct audio_module_handle_private *)handle, &msg_rx->audio_data,
			&audio_data);
		if (ret) {
			if (msg_rx->response_cb != NULL) {
				msg_rx->response_cb(
					(struct audio_module_handle_private *)(msg_rx->tx_handle),
					&msg_rx->audio_data);
			}

			data_fifo_block_free(handle->thread.msg_rx, (void **)(&msg_rx));

			k_mem_slab_free(handle->thread.data_slab, (void **)(&data));

			LOG_DBG("Data process error in module %s, ret %d", handle->name, ret);
			continue;
		}

		/* Send input audio data to next module(s). */
		send_to_connected_modules(handle, &audio_data);

		if (msg_rx->response_cb != NULL) {
			msg_rx->response_cb((struct audio_module_handle_private *)msg_rx->tx_handle,
					    &msg_rx->audio_data);
		}

		data_fifo_block_free(handle->thread.msg_rx, (void **)&msg_rx);
	}

	CODE_UNREACHABLE;
}

/**
 * @brief  Function for opening a module.
 */
int audio_module_open(struct audio_module_parameters *parameters,
		      struct audio_module_configuration *configuration, char *name,
		      struct audio_module_context *context, struct audio_module_handle *handle)
{
	int ret;
	k_thread_entry_t thread_entry;

	if (parameters == NULL || configuration == NULL || name == NULL || handle == NULL ||
	    context == NULL) {
		LOG_ERR("Parameter is NULL for module open function");
		return -EINVAL;
	}

	if (handle->state != AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_ERR("The module is already open");
		return -EALREADY;
	}

	ret = validate_parameters(parameters);
	if (ret) {
		LOG_ERR("Invalid parameters for module, ret %d", ret);
		return ret;
	}

	memset(handle, 0, sizeof(struct audio_module_handle));

	handle->description->type = parameters->description->type;

	/* Allocate the context memory. */
	handle->context = context;

	memcpy(handle->name, name, CONFIG_AUDIO_MODULE_NAME_SIZE - 1);
	if (strlen(name) > CONFIG_AUDIO_MODULE_NAME_SIZE - 1) {
		LOG_WRN("Module's instance name truncated to %s", handle->name);
	}
	handle->name[CONFIG_AUDIO_MODULE_NAME_SIZE - 1] = '\0';

	handle->description = parameters->description;
	memcpy(&handle->thread, &parameters->thread,
	       sizeof(struct audio_module_thread_configuration));

	if (handle->description->functions->open != NULL) {
		ret = handle->description->functions->open(
			(struct audio_module_handle_private *)handle, configuration);
		if (ret) {
			LOG_ERR("Failed open call to module %s, ret %d", name, ret);
			return ret;
		}
	}

	ret = handle->description->functions->configuration_set(
		(struct audio_module_handle_private *)handle, configuration);
	if (ret) {
		LOG_ERR("Set configuration for module %s send failed, ret %d", handle->name, ret);
		return ret;
	}

	switch (handle->description->type) {
	case AUDIO_MODULE_TYPE_INPUT:
		thread_entry = (k_thread_entry_t)module_thread_input;
		break;

	case AUDIO_MODULE_TYPE_OUTPUT:
		thread_entry = (k_thread_entry_t)module_thread_output;
		break;

	case AUDIO_MODULE_TYPE_IN_OUT:
		thread_entry = (k_thread_entry_t)module_thread_in_out;
		break;

	default:
		LOG_ERR("Invalid module type %d for module %s", handle->description->type,
			handle->name);
		return -EINVAL;
	}

	sys_slist_init(&handle->hdl_dest_list);
	k_mutex_init(&handle->dest_mutex);

	handle->thread_id = k_thread_create(
		&handle->thread_data, handle->thread.stack, handle->thread.stack_size,
		(k_thread_entry_t)module_thread_in_out, (void *)handle, NULL, NULL,
		K_PRIO_PREEMPT(handle->thread.priority), 0, K_FOREVER);

	ret = k_thread_name_set(handle->thread_id, &handle->name[0]);
	if (ret) {
		LOG_ERR("Failed to start thread for module %s thread, ret %d", handle->name, ret);

		/* Clean up the handle. */
		memset(handle, 0, sizeof(struct audio_module_handle));
		return ret;
	}

	handle->state = AUDIO_MODULE_STATE_CONFIGURED;

	k_thread_start(handle->thread_id);

	LOG_DBG("Thread started");

	return 0;
}

/**
 * @brief  Function to close an open module.
 */
int audio_module_close(struct audio_module_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_ERR("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED ||
	    handle->state == AUDIO_MODULE_STATE_RUNNING) {
		LOG_ERR("Module %s in an invalid state, %d, for close", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->close != NULL) {
		ret = handle->description->functions->close(
			(struct audio_module_handle_private *)handle);
		if (ret) {
			LOG_ERR("Failed close call to module %s, returned %d", handle->name, ret);
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
	 * Test the semaphore and wait for it to be zero.
	 */

	k_thread_abort(handle->thread_id);

	LOG_DBG("Closed module %s", handle->name);

	/* Clean up the handle. */
	memset(handle, 0, sizeof(struct audio_module_handle));

	return 0;
};

/**
 * @brief  Function to reconfigure a module.
 */
int audio_module_reconfigure(struct audio_module_handle *handle,
			     struct audio_module_configuration *configuration)
{
	int ret;

	if (handle == NULL || configuration == NULL) {
		LOG_ERR("Module input parameter error");
		return -EINVAL;
	}

	if (handle->description->functions->configuration_set == NULL) {
		LOG_ERR("Module %s has no reconfigure function", handle->name);
		return -ENOTSUP;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED ||
	    handle->state == AUDIO_MODULE_STATE_RUNNING) {
		LOG_ERR("Module %s in an invalid state, %d, for setting the configuration",
			handle->name, handle->state);
		return -ENOTSUP;
	}

	ret = handle->description->functions->configuration_set(
		(struct audio_module_handle_private *)handle, configuration);
	if (ret) {
		LOG_ERR("Set configuration for module %s send failed, ret %d", handle->name, ret);
		return ret;
	}

	handle->state = AUDIO_MODULE_STATE_CONFIGURED;

	return 0;
};

/**
 * @brief  Function to get the configuration of a module.
 */
int audio_module_configuration_get(struct audio_module_handle *handle,
				   struct audio_module_configuration *configuration)
{
	int ret;

	if (handle == NULL || configuration == NULL) {
		LOG_ERR("Module input parameter error");
		return -EINVAL;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_ERR("Module %s in an invalid state, %d, for getting the configuration",
			handle->name, handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->configuration_get != NULL) {
		ret = handle->description->functions->configuration_get(
			(struct audio_module_handle_private *)handle, configuration);
		if (ret) {
			LOG_DBG("Get configuration for module %s failed, ret %d", handle->name,
				ret);
			return ret;
		}
	} else {
		LOG_WRN("Get configuration for module %s has no get configuration function",
			handle->name);
	}

	return 0;
};

/**
 * @brief Function to connect two modules together.
 *
 */
int audio_module_connect(struct audio_module_handle *handle_from,
			 struct audio_module_handle *handle_to)
{
	int ret;
	struct audio_module_handle *handle;

	if (handle_from == NULL || handle_to == NULL) {
		LOG_ERR("Invalid parameter for the connection function");
		return -EINVAL;
	}

	ret = handle_connection_type_test(handle_from, handle_to);
	if (ret) {
		LOG_WRN("Connections between these modules, %s to %s, is not supported",
			handle_from->name, handle_to->name);
		return -ENOTSUP;
	}

	if (handle_from->state == AUDIO_MODULE_STATE_UNDEFINED ||
	    handle_to->state == AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_WRN("A module is in an invalid state for connecting modules %s to %s",
			handle_from->name, handle_to->name);
		return -ENOTSUP;
	}

	ret = k_mutex_lock(&handle_from->dest_mutex, LOCK_TIMEOUT_US);
	if (ret) {
		return ret;
	}

	if (handle_from == handle_to) {
		handle_from->data_tx = true;
		handle_from->dest_count += 1;
		LOG_DBG("Return the output of %s on it's TX message queue", handle_from->name);
	} else {
		SYS_SLIST_FOR_EACH_CONTAINER(&handle_from->hdl_dest_list, handle, node) {
			if (handle_to == handle) {
				LOG_WRN("Already attached %s to %s", handle_to->name,
					handle_from->name);
				return -EALREADY;
			}
		}

		sys_slist_append(&handle_from->hdl_dest_list, &handle_to->node);
		handle_from->dest_count += 1;

		LOG_DBG("Connected the output of %s to the input of %s", handle_from->name,
			handle_to->name);
	}

	k_mutex_unlock(&handle_from->dest_mutex);

	return 0;
}

/**
 * @brief Function to disconnect modules from each other.
 *
 */
int audio_module_disconnect(struct audio_module_handle *handle,
			    struct audio_module_handle *handle_disconnect)
{
	int ret;

	if (handle == NULL || handle_disconnect == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	ret = handle_connection_type_test(handle, handle_disconnect);
	if (ret) {
		LOG_WRN("Disconnection of modules %s from %s, is not supported",
			handle_disconnect->name, handle->name);
		return -ENOTSUP;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED ||
	    handle_disconnect->state == AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_WRN("A module is an invalid state or type for disconnection");
		return -ENOTSUP;
	}

	ret = k_mutex_lock(&handle->dest_mutex, LOCK_TIMEOUT_US);
	if (ret) {
		return ret;
	}

	if (handle == handle_disconnect) {
		handle->data_tx = false;
	} else {
		if (!sys_slist_find_and_remove(&handle->hdl_dest_list, &handle_disconnect->node)) {
			LOG_ERR("Connection to module %s has not been found for module %s",
				handle_disconnect->name, handle->name);
			return -EALREADY;
		}

		LOG_DBG("Disconnect module %s from module %s", handle_disconnect->name,
			handle->name);
	}

	handle->dest_count -= 1;

	k_mutex_unlock(&handle->dest_mutex);

	return 0;
}

/**
 * @brief Start processing data in the module given by handle.
 */
int audio_module_start(struct audio_module_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_WRN("Module %s in an invalid state, %d, for start", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	if (handle->state == AUDIO_MODULE_STATE_RUNNING) {
		LOG_DBG("Module %s already running", handle->name);
		return -EALREADY;
	}

	if (handle->description->functions->start != NULL) {
		ret = handle->description->functions->start(
			(struct audio_module_handle_private *)handle);
		if (ret < 0) {
			LOG_ERR("Failed user start for module %s, ret %d", handle->name, ret);
			return ret;
		}
	}

	handle->state = AUDIO_MODULE_STATE_RUNNING;

	return 0;
}

/**
 * @brief Stop processing data in the module given by handle.
 */
int audio_module_stop(struct audio_module_handle *handle)
{
	int ret;

	if (handle == NULL) {
		LOG_ERR("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state == AUDIO_MODULE_STATE_STOPPED) {
		LOG_DBG("Module %s already stopped", handle->name);
		return -EALREADY;
	}

	if (handle->state != AUDIO_MODULE_STATE_RUNNING) {
		LOG_WRN("Module %s in an invalid state, %d, for stop", handle->name, handle->state);
		return -ENOTSUP;
	}

	if (handle->description->functions->stop != NULL) {
		ret = handle->description->functions->stop(
			(struct audio_module_handle_private *)handle);
		if (ret < 0) {
			LOG_ERR("Failed user pause for module %s, ret %d", handle->name, ret);
			return ret;
		}
	}

	handle->state = AUDIO_MODULE_STATE_STOPPED;

	return 0;
}

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 */
int audio_module_data_tx(struct audio_module_handle *handle, struct audio_data *audio_data,
			 audio_module_response_cb response_cb)
{
	if (handle == NULL) {
		LOG_ERR("Module handle is NULL");
		return -EINVAL;
	}

	if (handle->state != AUDIO_MODULE_STATE_RUNNING ||
	    handle->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle->description->type == AUDIO_MODULE_TYPE_INPUT) {
		LOG_WRN("Module %s in an invalid state (%d) or type (%d) to transmit data",
			handle->name, handle->state, handle->description->type);
		return -ENOTSUP;
	}

	if (handle->thread.msg_rx == NULL) {
		LOG_ERR("Module has message queue set to NULL");
		return -ENOTSUP;
	}

	if (audio_data == NULL || audio_data->data == NULL || audio_data->data_size == 0) {
		LOG_ERR("Module , %s, data parameter error", handle->name);
		return -ECONNREFUSED;
	}

	return data_tx((void *)NULL, handle, audio_data, response_cb);
}

/**
 * @brief Retrieve data from the module.
 *
 */
int audio_module_data_rx(struct audio_module_handle *handle, struct audio_data *audio_data,
			 k_timeout_t timeout)
{
	int ret;

	struct audio_module_message *msg_tx;
	size_t msg_tx_size;

	if (handle == NULL) {
		LOG_ERR("Module handle error");
		return -EINVAL;
	}

	if (handle->state != AUDIO_MODULE_STATE_RUNNING ||
	    handle->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle->description->type == AUDIO_MODULE_TYPE_OUTPUT) {
		LOG_WRN("Module %s in an invalid state (%d) or type (%d) to receive data",
			handle->name, handle->state, handle->description->type);
		return -ENOTSUP;
	}

	if (handle->thread.msg_tx == NULL) {
		LOG_ERR("Module has message queue set to NULL");
		return -ENOTSUP;
	}

	if (audio_data == NULL || audio_data->data == NULL || audio_data->data_size == 0) {
		LOG_ERR("Error in audio data for module %s", handle->name);
		return -ECONNREFUSED;
	}

	ret = data_fifo_pointer_last_filled_get(handle->thread.msg_tx, (void **)&msg_tx,
						&msg_tx_size, timeout);
	if (ret) {
		LOG_ERR("Failed to retrieve data from module %s, ret %d", handle->name, ret);
		return ret;
	}

	if (msg_tx->audio_data.data == NULL ||
	    msg_tx->audio_data.data_size > audio_data->data_size) {
		LOG_ERR("Data output buffer NULL or too small for received buffer from module %s",
			handle->name);
		ret = -EINVAL;
	} else {
		memcpy(&audio_data->meta, &msg_tx->audio_data.meta, sizeof(struct audio_metadata));
		memcpy((uint8_t *)audio_data->data, (uint8_t *)msg_tx->audio_data.data,
		       msg_tx->audio_data.data_size);
	}

	audio_data_release_cb((struct audio_module_handle_private *)handle, &msg_tx->audio_data);

	data_fifo_block_free(handle->thread.msg_tx, (void **)&msg_tx);

	return ret;
}

/**
 * @brief Send an audio data to a module and retrieve an audio data from a module.
 *
 * @note The audio data is processed within the module or sequence of modules. The result is
 * returned via the module or final module's output FIFO. All the input data is consumed within the
 * call and thus the input data buffer maybe released once the function returns.
 *
 */
int audio_module_data_tx_rx(struct audio_module_handle *handle_tx,
			    struct audio_module_handle *handle_rx, struct audio_data *audio_data_tx,
			    struct audio_data *audio_data_rx, k_timeout_t timeout)
{
	int ret;
	struct audio_module_message *msg_rx;
	size_t msg_rx_size;

	if (handle_tx == NULL || handle_rx == NULL) {
		LOG_ERR("Module handle is NULL");
		return -EINVAL;
	}

	if (handle_tx->state != AUDIO_MODULE_STATE_RUNNING ||
	    handle_rx->state != AUDIO_MODULE_STATE_RUNNING) {
		LOG_WRN("Module is in an invalid state or type to send-receive data");
		return -ENOTSUP;
	}

	if (handle_tx->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle_tx->description->type == AUDIO_MODULE_TYPE_INPUT ||
	    handle_rx->description->type == AUDIO_MODULE_TYPE_UNDEFINED ||
	    handle_rx->description->type == AUDIO_MODULE_TYPE_OUTPUT) {
		LOG_WRN("Module not of the right type for operation");
		return -ENOTSUP;
	}

	if (audio_data_tx == NULL || audio_data_rx == NULL) {
		LOG_WRN("Block pointer for module is NULL");
		return -ECONNREFUSED;
	}

	if (audio_data_tx->data == NULL || audio_data_tx->data_size == 0 ||
	    audio_data_rx->data == NULL || audio_data_rx->data_size == 0) {
		LOG_WRN("Invalid output audio data");
		return -ECONNREFUSED;
	}

	if (handle_tx->thread.msg_rx == NULL || handle_rx->thread.msg_tx == NULL) {
		LOG_ERR("Modules have message queue set to NULL");
		return -ENOTSUP;
	}

	ret = data_tx(NULL, handle_tx, audio_data_tx, NULL);
	if (ret) {
		LOG_ERR("Failed to send data to module %s, ret %d", handle_tx->name, ret);
		return ret;
	}

	LOG_DBG("Wait for message on module %s TX queue", handle_rx->name);

	ret = data_fifo_pointer_last_filled_get(handle_rx->thread.msg_rx, (void **)&msg_rx,
						&msg_rx_size, timeout);
	if (ret) {
		LOG_ERR("Failed to retrieve data from module %s, ret %d", handle_rx->name, ret);
		return ret;
	}

	if (msg_rx->audio_data.data == NULL || msg_rx->audio_data.data_size == 0) {
		LOG_ERR("Data output buffer too small for received buffer from module %s (%d)",
			handle_rx->name, msg_rx->audio_data.data_size);
		ret = -EINVAL;
	} else {
		memcpy(&audio_data_rx->meta, &msg_rx->audio_data.meta,
		       sizeof(struct audio_metadata));
		memcpy((uint8_t *)audio_data_rx->data, (uint8_t *)msg_rx->audio_data.data,
		       msg_rx->audio_data.data_size);
	}

	if (handle_tx != handle_rx) {
		audio_data_release_cb((struct audio_module_handle_private *)handle_rx,
				      &msg_rx->audio_data);
	}

	data_fifo_block_free(handle_rx->thread.msg_rx, (void **)&msg_rx);

	return ret;
};

/**
 * @brief Helper function to return the base and instance names for a given
 *        module handle.
 *
 */
int audio_module_names_get(struct audio_module_handle *handle, char **base_name,
			   char *instance_name)
{
	if (handle == NULL || base_name == NULL || instance_name == NULL) {
		LOG_ERR("Input parameter is NULL");
		return -EINVAL;
	}

	if (handle->state == AUDIO_MODULE_STATE_UNDEFINED) {
		LOG_WRN("Module %s is in an invalid state, %d, for get names", handle->name,
			handle->state);
		return -ENOTSUP;
	}

	*base_name = handle->description->name;
	memcpy(instance_name, &handle->name, CONFIG_AUDIO_MODULE_NAME_SIZE);

	return 0;
}

/**
 * @brief Helper function to get the state of a given module handle.
 *
 */
int audio_module_state_get(struct audio_module_handle *handle, enum audio_module_state *state)
{
	if (handle == NULL || state == NULL) {
		LOG_ERR("Input parameter is NULL");
		return -EINVAL;
	}

	if (handle->state > AUDIO_MODULE_STATE_STOPPED) {
		LOG_WRN("Module state is invalid");
		return -EINVAL;
	}

	*state = handle->state;

	return 0;
};

/**
 * @brief Calculate the number of channels from the channel map for the given module handle.
 *
 */
int audio_module_number_channels_calculate(uint32_t locations, int8_t *number_channels)
{
	if (number_channels == NULL) {
		LOG_ERR("Invalid parameters");
		return -EINVAL;
	}

	*number_channels = 0;
	for (int i = 0; i < AUDIO_MODULE_BITS_IN_CHANNEL_MAP; i++) {
		*number_channels += locations & 1;
		locations >>= 1;
	}

	LOG_DBG("Found %d channel(s)", *number_channels);

	return 0;
}
