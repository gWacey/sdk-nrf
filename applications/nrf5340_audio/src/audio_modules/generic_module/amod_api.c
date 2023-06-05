/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "amod_api.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <nrfx_clock.h>
#include "data_fifo.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(module_api, 4); /* CONFIG_MODULE_API_LOG_LEVEL); */

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

	if ((parameters->description->type != AMOD_TYPE_INPUT &&
	     parameters->description->type != AMOD_TYPE_OUTPUT &&
	     parameters->description->type != AMOD_TYPE_PROCESSOR) ||
	    parameters->description->name == NULL || parameters->description->functions == NULL) {
		LOG_DBG("Error in description for module");
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief General callback for releasing the audio object when intermodule object
 *        passing.
 *
 * @param handle  The handle of the sending modules instance
 * @param object  Pointer to the audio data object to send to the module
 */
void object_release(struct amod_handle *handle, struct aobj_object *object)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;

	k_sem_take(&hdl->sem, K_NO_WAIT);

	if (k_sem_count_get(&hdl->sem) == 0) {
		/* Object data has been consumed by all modules so now can free the memory */
		k_mem_slab_free(&hdl->data_slab, (void **)object->data);
	}
}

/**
 * @brief Send a data buffer to a module, all data is consumed by the module.
 *
 * @param tx_handle    The handle for the sending module instance
 * @param rx_handle    The handle for the receiving module instance
 * @param object       Pointer to the audio data object to send to the module
 * @param response_cb  A pointer to a callback to run when the buffer is
 *                     fully comsumed
 *
 * @return 0 if successful, error value
 */
static int data_tx(struct amod_handle *tx_handle, struct amod_handle *rx_handle,
		   struct aobj_object *object, amod_response_cb data_in_response_cb)
{
	int ret;
	struct _amod_message *data_in_msg;

	ret = data_fifo_pointer_first_vacant_get(&rx_handle->in_msg, (void **)&data_in_msg,
						 K_NO_WAIT);
	if (ret) {
		LOG_DBG("Module %s no free data buffer, ret %d", rx_handle->name, ret);
		return ret;
	}

	/* fill data */
	memcpy(&data_in_msg->object, object, sizeof(struct aobj_object));
	data_in_msg->tx_handle = (struct amod_handle *)tx_handle;
	data_in_msg->response_cb = data_in_response_cb;

	ret = data_fifo_block_lock(&rx_handle->in_msg, (void **)&data_in_msg,
				   sizeof(struct _amod_message));
	if (ret) {
		data_fifo_block_free(&rx_handle->in_msg, (void **)&data_in_msg);

		LOG_DBG("Module %s failed to return output buffer, ret %d", rx_handle->name, ret);
		return ret;
	}

	LOG_DBG("Data sent to module %s", rx_handle->name);

	return 0;
}

/**
 * @brief Function to clean up items if a procedure fails.
 *
 */
static void clean_up(struct amod_handle *hdl, struct _amod_message **in_msg,
		     struct _amod_message **out_msg, char **data)
{
	if ((*in_msg)->response_cb != NULL) {
		(*in_msg)->response_cb((*in_msg)->tx_handle, &(*in_msg)->object);
	}

	if ((*in_msg) != NULL) {
		data_fifo_block_free(&hdl->in_msg, (void **)(*in_msg));
	}

	if ((*out_msg) != NULL) {
		data_fifo_block_free(&hdl->out_msg, (void **)(*out_msg));
	}

	if ((*data) != NULL) {
		k_mem_slab_free(&hdl->data_slab, (void **)(*data));
	}
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
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct amod_handle *hdl_to;
	struct _amod_message *in_msg = NULL;
	struct _amod_message *out_msg;
	char *data;

	if (handle == NULL) {
		LOG_DBG("Error in the module task as handle NULL");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		out_msg = NULL;
		data = NULL;

		if (hdl->description.functions->data_process != NULL) {
			/* Get a new output buffer */
			ret = k_mem_slab_alloc(&hdl->data_slab, (void **)&data, K_FOREVER);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("No free data buffer for module %st, ret %d", hdl->name,
					ret);
				continue;
			}

			ret = data_fifo_pointer_first_vacant_get(&hdl->out_msg, (void **)&out_msg,
								 K_FOREVER);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("No free message for module %s, ret %d", hdl->name, ret);
				continue;
			}

			/* Configure new audio object */
			out_msg->object.data = data;
			out_msg->object.data_size = hdl->data_size;

			/* Process the input audio object */
			ret = hdl->description.functions->data_process(handle, NULL,
								       &out_msg->object);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("Data process error in module %s, ret %d", hdl->name, ret);
				continue;
			}

			/* Send input audio object to next module(s) */
			if (!sys_slist_is_empty(&hdl->hdl_dest_list)) {
				k_mutex_lock(&hdl->dest_mutex, K_FOREVER);
				k_sem_init(&hdl->sem, hdl->dest_count, hdl->dest_count);

				SYS_SLIST_FOR_EACH_CONTAINER(&hdl->hdl_dest_list, hdl_to, node) {
					ret = data_tx(hdl, hdl_to, &out_msg->object,
						      &object_release);
					if (ret) {
						LOG_DBG("Failed to send to module %s from %s",
							hdl_to->name, hdl->name);
						k_sem_take(&hdl->sem, K_NO_WAIT);
					}
				}

				k_mutex_unlock(&hdl->dest_mutex);
			}

			if (hdl->msg_out) {
				/* Send audio object to modules output message queue */
				ret = data_fifo_block_lock(&hdl->out_msg, (void **)&out_msg,
							   sizeof(struct _amod_message));
				if (ret) {
					clean_up(hdl, &in_msg, &out_msg, &data);

					LOG_DBG("Module %s failed to return output buffer, ret %d",
						hdl->name, ret);

					k_sem_take(&hdl->sem, K_NO_WAIT);
					continue;
				}
			}
		} else {
			LOG_DBG("No process function for module %s, discarding input", hdl->name);
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
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct _amod_message *in_msg;
	struct _amod_message *out_msg = NULL;
	char *data = NULL;
	size_t size;

	if (handle == NULL) {
		LOG_DBG("Error in the module task as handle NULL");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		in_msg = NULL;

		ret = data_fifo_pointer_last_filled_get(&hdl->in_msg, (void **)&in_msg, &size,
							K_FOREVER);
		if (ret) {
			LOG_DBG("No new data for module %s, ret %d", hdl->name, ret);
			continue;
		}

		if (hdl->description.functions->data_process != NULL) {
			/* Process the input audio object and output from the audio system */
			ret = hdl->description.functions->data_process(handle, &in_msg->object,
								       NULL);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("Data process error in module %s, ret %d", hdl->name, ret);
				continue;
			}

			if (in_msg->response_cb != NULL) {
				in_msg->response_cb(in_msg->tx_handle, &in_msg->object);
			}

		} else {
			LOG_DBG("No process function for module %s, discarding input", hdl->name);
		}

		data_fifo_block_free(&hdl->in_msg, (void **)&in_msg);
	}

	return 0;
}

/**
 * @brief The thread that processes inputs and returns data from the module
 *
 * @param handle  The handle for this modules instance
 *
 * @return 0 if successful, error value
 */
static int module_thread_processor(struct amod_handle *handle)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct amod_handle *hdl_to;
	struct _amod_message *in_msg;
	struct _amod_message *out_msg;
	char *data = NULL;
	size_t size;

	if (handle == NULL) {
		LOG_DBG("Error in the module task as handle NULL");
		return -EINVAL;
	}

	/* Execute thread */
	while (1) {
		in_msg = NULL;
		out_msg = NULL;
		data = NULL;

		ret = data_fifo_pointer_last_filled_get(&hdl->in_msg, (void **)&in_msg, &size,
							K_FOREVER);
		if (ret) {
			LOG_DBG("No new message for module %s, ret %d", hdl->name, ret);
			continue;
		}

		if (hdl->description.functions->data_process != NULL) {
			/* Get a new output buffer from outside the audio system */
			ret = k_mem_slab_alloc(&hdl->data_slab, (void **)&data, K_NO_WAIT);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("No free data buffer for module %s, dropping input, ret %d",
					hdl->name, ret);
				continue;
			}

			ret = data_fifo_pointer_first_vacant_get(&hdl->out_msg, (void **)&out_msg,
								 K_NO_WAIT);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("No free message for module %s, dropping input, ret %d",
					hdl->name, ret);
				continue;
			}

			/* Configure new audio object */
			out_msg->object.data = data;
			out_msg->object.data_size = hdl->data_size;

			/* Process the input audio object into the output audio object */
			ret = hdl->description.functions->data_process(handle, &in_msg->object,
								       &out_msg->object);
			if (ret) {
				clean_up(hdl, &in_msg, &out_msg, &data);

				LOG_DBG("Data process error in module %s, ret %d", hdl->name, ret);
				continue;
			}

			/* Send output audio object to next module(s) */
			if (!sys_slist_is_empty(&hdl->hdl_dest_list)) {
				k_mutex_lock(&hdl->dest_mutex, K_FOREVER);
				k_sem_init(&hdl->sem, hdl->dest_count, hdl->dest_count);

				SYS_SLIST_FOR_EACH_CONTAINER(&hdl->hdl_dest_list, hdl_to, node) {
					ret = data_tx(hdl, hdl_to, &out_msg->object,
						      &object_release);
					if (ret) {
						LOG_DBG("Failed to send to module %s from %s",
							hdl_to->name, hdl->name);

						k_sem_take(&hdl->sem, K_NO_WAIT);
					}
				}

				k_mutex_unlock(&hdl->dest_mutex);
			}

			if (hdl->msg_out) {
				/* Send audio object to modules output message queue */
				ret = data_fifo_block_lock(&hdl->out_msg, (void **)&out_msg,
							   sizeof(struct _amod_message));
				if (ret) {
					clean_up(hdl, &in_msg, &out_msg, &data);

					LOG_DBG("Module %s failed to return output buffer, ret %d",
						hdl->name, ret);

					k_sem_take(&hdl->sem, K_NO_WAIT);
					continue;
				}
			}

			if (in_msg->response_cb != NULL) {
				in_msg->response_cb(in_msg->tx_handle, &in_msg->object);
			}

		} else {
			LOG_DBG("No process function for module %s, discarding input", hdl->name);
		}

		data_fifo_block_free(&hdl->in_msg, (void **)&in_msg);
	}

	return 0;
}
/**
 * @brief  Function for querying the resources required for a module.
 */
int amod_query_resource(struct amod_parameters *parameters,
			struct amod_configuration *configuration)
{
	int ret;
	int size = 0;

	ret = validate_parameters(parameters);
	if (ret) {
		LOG_DBG("Invalid parameters for module, returned %d", ret);
		return ret;
	}

	if (parameters->description->functions->query_resource != NULL) {
		size = parameters->description->functions->query_resource(configuration);
		if (size < 0) {
			LOG_DBG("Failed query resource for module %s, ret %d",
				parameters->description->name, size);
			return -EFAULT;
		}
	} else {
		LOG_DBG("No query resource function for module %s", parameters->description->name);
	}

	size += WB_UP(sizeof(struct amod_handle));

	return size;
};

/**
 * @brief  Function for allocating memory for the module.
 */
int amod_memory_allocate(void)
{
	return 0;
}

/**
 * @brief  Function for de-allocating memory for the module.
 */
int amod_memory_deallocate(void)
{
	return 0;
}

/**
 * @brief  Function for opening a module.
 */
int amod_open(struct amod_parameters *parameters, struct amod_configuration *configuration,
	      char *name, char *in_msg_block, char *out_msg_block, char *data_block,
	      size_t data_size, uint32_t data_num, struct amod_handle *handle)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Input handle parameter is NULL for module %s in the open function", name);
		return -EINVAL;
	}

	if (hdl->state == AMOD_STATE_OPEN) {
		LOG_DBG("Module %s is already open", hdl->name);
		return -EALREADY;
	}

	ret = validate_parameters(parameters);
	if (ret) {
		LOG_DBG("Invalid parameters for module, ret %d", ret);
		return ret;
	}

	memset(handle, 0, sizeof(struct amod_handle));

	hdl->description.type = parameters->description->type;

	if (hdl->description.type == AMOD_TYPE_INPUT && out_msg_block == NULL &&
	    data_block == NULL) {
		LOG_DBG("Incorrect memory for module %s in the open function", name);
		return -EINVAL;
	}

	if (hdl->description.type == AMOD_TYPE_OUTPUT && in_msg_block == NULL) {
		LOG_DBG("Incorrect memory for module %s in the open function", name);
		return -EINVAL;
	}

	if (in_msg_block == NULL && out_msg_block == NULL && data_block == NULL) {
		LOG_DBG("Incorrect memory for module %s in the open function", name);
		return -EINVAL;
	}

	hdl->previous_state = AMOD_STATE_UNDEFINED;
	hdl->state = AMOD_STATE_UNDEFINED;

	/* Allocate the context memory */
	hdl->context = (struct amod_context *)((char *)handle + WB_UP(sizeof(struct amod_handle)));

	memcpy(hdl->name, name, AMOD_NAME_SIZE);
	memcpy(&hdl->description, parameters->description, sizeof(struct amod_description));
	memcpy(&hdl->thread, &parameters->thread, sizeof(struct amod_thread_configuration));

	sys_slist_init(&hdl->hdl_dest_list);
	k_mutex_init(&hdl->dest_mutex);

	if (hdl->description.functions->open != NULL) {
		ret = hdl->description.functions->open((struct amod_handle *)hdl, configuration);
		if (ret) {
			LOG_DBG("Failed open call to module %s, ret %d", name, ret);
			return ret;
		}
	}

	if (hdl->description.type != AMOD_TYPE_INPUT) {
		hdl->in_msg.msgq_buffer = in_msg_block;
		hdl->in_msg.slab_buffer = in_msg_block + AMOD_MSG_SET_SIZE(hdl->thread.in_msg_num);
		hdl->in_msg.block_size_max = sizeof(struct _amod_message);
		hdl->in_msg.elements_max = hdl->thread.in_msg_num;
		hdl->in_msg.initialized = false;
		ret = data_fifo_init(&hdl->in_msg);
		if (ret) {
			LOG_DBG("Failed to initialise module %s data in FIFO, ret %d", hdl->name,
				ret);
			return ret;
		}
	}

	if (hdl->description.type != AMOD_TYPE_OUTPUT) {
		hdl->out_msg.msgq_buffer = out_msg_block;
		hdl->out_msg.slab_buffer =
			out_msg_block + AMOD_MSG_SET_SIZE(hdl->thread.out_msg_num);
		hdl->out_msg.block_size_max = sizeof(struct _amod_message);
		hdl->out_msg.elements_max = hdl->thread.out_msg_num;
		hdl->out_msg.initialized = false;
		ret = data_fifo_init(&hdl->out_msg);
		if (ret) {
			LOG_DBG("Failed to initialise module %s data output FIFO, ret %d",
				hdl->name, ret);
			return ret;
		}

		hdl->data_size = WB_UP(hdl->data_size);
		ret = k_mem_slab_init(&hdl->data_slab, data_block, hdl->data_size, data_num);
		if (ret) {
			LOG_DBG("Failed to initialise module %s data slab, ret %d", hdl->name, ret);
			return ret;
		}
	}

	switch (hdl->description.type) {
	case AMOD_TYPE_INPUT:
		hdl->thread_id =
			k_thread_create(&hdl->thread_data, hdl->thread.stack,
					hdl->thread.stack_size,
					(k_thread_entry_t)module_thread_input, hdl, NULL, NULL,
					K_PRIO_PREEMPT(hdl->thread.priority), 0, K_NO_WAIT);

		ret = k_thread_name_set(hdl->thread_id, hdl->name);
		if (ret) {
			LOG_DBG("Failed to start input module %s thread, ret %d", hdl->name, ret);
			return ret;
		}

		break;

	case AMOD_TYPE_OUTPUT:
		hdl->thread_id =
			k_thread_create(&hdl->thread_data, hdl->thread.stack,
					hdl->thread.stack_size,
					(k_thread_entry_t)module_thread_output, hdl, NULL, NULL,
					K_PRIO_PREEMPT(hdl->thread.priority), 0, K_NO_WAIT);

		ret = k_thread_name_set(hdl->thread_id, hdl->name);
		if (ret) {
			LOG_DBG("Failed to start output module %s thread, ret %d", hdl->name, ret);
			return ret;
		}

		break;

	case AMOD_TYPE_PROCESSOR:
		hdl->thread_id =
			k_thread_create(&hdl->thread_data, hdl->thread.stack,
					hdl->thread.stack_size,
					(k_thread_entry_t)module_thread_processor, hdl, NULL, NULL,
					K_PRIO_PREEMPT(hdl->thread.priority), 0, K_NO_WAIT);

		ret = k_thread_name_set(hdl->thread_id, hdl->name);
		if (ret) {
			LOG_DBG("Failed to start processor module %s thread, ret %d", hdl->name,
				ret);
			return ret;
		}

		break;

	default:
		LOG_DBG("Invalid module type %d for module %s", hdl->description.type, hdl->name);
		return -EINVAL;
	}

	hdl->state = AMOD_STATE_OPEN;

	LOG_DBG("Module %s is now open", hdl->name);

	return 0;
}

/**
 * @brief  Function to close an open module.
 */
int amod_close(struct amod_handle *handle)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl->state == AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s in an invalid state, %d, for close", hdl->name, hdl->state);
		return -ENOTSUP;
	}

	if (hdl->description.functions->close != NULL) {
		ret = hdl->description.functions->close(handle);
		if (ret) {
			LOG_DBG("Failed close call to module %s, returned %d", hdl->name, ret);
			return ret;
		}
	} else {
		LOG_DBG("Module %s has no close function", hdl->name);
	}

	data_fifo_empty(&hdl->in_msg);
	data_fifo_empty(&hdl->out_msg);

	k_thread_abort(hdl->thread_id);

	return 0;
};

/**
 * @brief  Function to set the configuration of a module.
 */
int amod_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL || configuration == NULL) {
		LOG_DBG("Module input parameter error");
		return -EINVAL;
	}

	if (hdl->state == AMOD_STATE_UNDEFINED || hdl->state == AMOD_STATE_OPEN) {
		LOG_DBG("Module %s in an invalid state, %d, for setting the configuration",
			hdl->name, hdl->state);
		return -ENOTSUP;
	}

	if (hdl->description.functions->configuration_set != NULL) {
		ret = hdl->description.functions->configuration_set(handle, configuration);
		if (ret) {
			LOG_DBG("Set configuration for module %s send failed, ret %d", hdl->name,
				ret);
			return ret;
		}
	} else {
		LOG_DBG("No set configuration function for module %s", hdl->name);
	}

	hdl->previous_state = hdl->state;
	hdl->state = AMOD_STATE_CONFIGURED;

	return 0;
};

/**
 * @brief  Function to get the configuration of a module.
 */
int amod_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL || configuration == NULL) {
		LOG_DBG("Module input parameter error");
		return -EINVAL;
	}

	if (hdl->state == AMOD_STATE_UNDEFINED || hdl->state == AMOD_STATE_OPEN) {
		LOG_DBG("Module %s in an invalid state, %d, for getting the configuration",
			hdl->name, hdl->state);
		return -ENOTSUP;
	}

	if (hdl->description.functions->configuration_get != NULL) {
		ret = hdl->description.functions->configuration_get(handle, configuration);
		if (ret) {
			LOG_DBG("Get configuration for module %s failed, ret %d", hdl->name, ret);
			return ret;
		}
	} else {
		LOG_DBG("No get configuration function for module %s", hdl->name);
		return -ENOSYS;
	}

	return 0;
};

/**
 * @brief Function to connect two modules together.
 *
 */
int amod_connect(struct amod_handle *handle_from, struct amod_handle *handle_to)
{
	struct amod_handle *hdl_from = (struct amod_handle *)handle_from;
	struct amod_handle *hdl_to = (struct amod_handle *)handle_to;

	if (handle_from == NULL || handle_to == NULL) {
		LOG_DBG("Invalid paramet for the connection function");
		return -EINVAL;
	}

	if (hdl_from->description.type == AMOD_TYPE_OUTPUT ||
	    hdl_to->description.type == AMOD_TYPE_INPUT) {
		LOG_DBG("Connections between these modules, %s to %s, is not supported",
			hdl_from->name, hdl_to->name);
		return -ENOTSUP;
	}

	if (hdl_from->state == AMOD_STATE_RUNNING || hdl_to->state == AMOD_STATE_RUNNING) {
		LOG_DBG("A module is in an invalid state for connecting modules %s to %s",
			hdl_from->name, hdl_to->name);
		return -ENOTSUP;
	}

	k_mutex_lock(&hdl_from->dest_mutex, K_FOREVER);

	if (handle_from == handle_to) {
		hdl_from->msg_out = true;
		hdl_from->dest_count += 1;
	} else {
		sys_slist_append(&hdl_from->hdl_dest_list, &hdl_to->node);
		hdl_from->dest_count += 1;
	}

	k_mutex_unlock(&hdl_from->dest_mutex);

	return 0;
}

/**
 * @brief Function to disconnect a module.
 *
 */
int amod_disconnect(struct amod_handle *handle, struct amod_handle *handle_disconnect)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct amod_handle *hdl_remove = (struct amod_handle *)handle_disconnect;

	if (handle == NULL || handle_disconnect == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl->description.type == AMOD_TYPE_OUTPUT ||
	    hdl_remove->description.type == AMOD_TYPE_INPUT) {
		LOG_DBG("Connections between these modules, %s to %s, is not supported", hdl->name,
			hdl_remove->name);
		return -ENOTSUP;
	}

	if (hdl->state == AMOD_STATE_UNDEFINED || hdl->state == AMOD_STATE_RUNNING ||
	    hdl->state == AMOD_STATE_OPEN) {
		LOG_DBG("A module is an invalid state or type for disconnection");
		return -ENOTSUP;
	}

	k_mutex_lock(&hdl->dest_mutex, K_FOREVER);

	if (!sys_slist_find_and_remove(&hdl->hdl_dest_list, &hdl_remove->node)) {
		LOG_DBG("Connection of module %s has not been found for module %s",
			hdl_remove->name, hdl->name);
		return -EINVAL;
	}

	hdl->dest_count -= 1;

	k_mutex_unlock(&hdl->dest_mutex);

	return 0;
}

/**
 * @brief Start a module processing data.
 */
int amod_start(struct amod_handle *handle)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl->state == AMOD_STATE_UNDEFINED || hdl->state == AMOD_STATE_RUNNING ||
	    hdl->state == AMOD_STATE_OPEN) {
		LOG_DBG("Module %s in an invalid state, %d, for set configuration", hdl->name,
			hdl->state);
		return -ENOTSUP;
	}

	if (hdl->description.functions->start != NULL) {
		ret = hdl->description.functions->start(handle);
		if (ret < 0) {
			LOG_DBG("Failed user start for module %s, ret %d", hdl->name, ret);
			return ret;
		}
	} else {
		LOG_DBG("No user start function for module %s", hdl->name);
	}

	hdl->previous_state = hdl->state;
	hdl->state = AMOD_STATE_RUNNING;

	return 0;
}

/**
 * @brief Pause a module processing data.
 */
int amod_pause(struct amod_handle *handle)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl->state != AMOD_STATE_RUNNING) {
		LOG_DBG("Module %s in an invalid state, %d, for set configuration", hdl->name,
			hdl->state);
		return -ENOTSUP;
	}

	if (hdl->description.functions->pause != NULL) {
		ret = hdl->description.functions->pause(handle);
		if (ret < 0) {
			LOG_DBG("Failed user pause for module %s, ret %d", hdl->name, ret);
			return ret;
		}
	} else {
		LOG_DBG("No user pause function for module %s", hdl->name);
	}

	hdl->previous_state = hdl->state;
	hdl->state = AMOD_STATE_PAUSED;

	return 0;
}

/**
 * @brief Send a data buffer to a module.
 */
int amod_data_tx(struct amod_handle *handle, struct aobj_object *object,
		 amod_response_cb response_cb)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (object == NULL) {
		LOG_DBG("Module , %s, data parameter error", hdl->name);
		return -ECONNREFUSED;
	}

	if (hdl->state != AMOD_STATE_RUNNING || hdl->description.type == AMOD_TYPE_INPUT) {
		LOG_DBG("Module %s in an invalid state (%d) or type (%d) to send data", hdl->name,
			hdl->state, hdl->description.type);
		return -ENOTSUP;
	}

	return data_tx((void *)NULL, hdl, object, response_cb);
}

/**
 * @brief Retrieve data from the module.
 *
 */
int amod_data_rx(struct amod_handle *handle, struct aobj_object *object, k_timeout_t timeout)
{
	int ret;
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct _amod_message *out_msg;
	size_t out_msg_size;

	if (handle == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl->state != AMOD_STATE_RUNNING || hdl->description.type == AMOD_TYPE_OUTPUT) {
		LOG_DBG("Module %s in an invalid state (%d) or type (%d) to receive data",
			hdl->name, hdl->state, hdl->description.type);
		return -ENOTSUP;
	}

	if (object == NULL) {
		LOG_DBG("Error in output object for module %s", hdl->name);
		return -ECONNREFUSED;
	}

	ret = data_fifo_pointer_last_filled_get(&hdl->out_msg, (void **)&out_msg, &out_msg_size,
						timeout);
	if (ret) {
		LOG_DBG("Failed to retrieve data from module %s, ret %d", hdl->name, ret);
		return ret;
	}

	if (object->data == NULL || out_msg->object.data_size > object->data_size) {
		LOG_DBG("Data output buffer too small for received buffer from module %s",
			hdl->name);
		ret = -EINVAL;
	} else {
		uint8_t *data_out = object->data;

		memcpy(object, &out_msg->object, sizeof(struct aobj_object));
		memcpy(data_out, out_msg->object.data, out_msg->object.data_size);

		ret = 0;
	}

	object_release(handle, &out_msg->object);

	data_fifo_block_free(&hdl->out_msg, (void **)&out_msg);

	return ret;
}

/**
 * @brief Send data and retrieve the processed data from the module.
 */
int amod_data_rx_tx(struct amod_handle *handle_tx, struct amod_handle *handle_rx,
		    struct aobj_object *object_tx, struct aobj_object *object_rx,
		    k_timeout_t timeout)
{
	int ret;
	struct amod_handle *hdl_tx = (struct amod_handle *)handle_tx;
	struct amod_handle *hdl_rx = (struct amod_handle *)handle_rx;
	struct _amod_message *out_msg;
	size_t out_msg_size;

	if (handle_tx == NULL || handle_rx == NULL) {
		LOG_DBG("Module handle is NULL");
		return -EINVAL;
	}

	if (hdl_tx->state != AMOD_STATE_RUNNING || hdl_rx->state != AMOD_STATE_RUNNING) {
		LOG_DBG("Module is in an invalid state or type to send-receive data");
		return -ENOTSUP;
	}

	if (hdl_tx->description.type == AMOD_TYPE_INPUT ||
	    hdl_rx->description.type == AMOD_TYPE_OUTPUT) {
		LOG_DBG("Module noyt of the right type for operation");
		return -EINVAL;
	}

	if (object_tx == NULL || object_rx == NULL) {
		LOG_DBG("Error in audio object pointer for module");
		return -ECONNREFUSED;
	}

	if (object_tx->data == NULL || object_tx->data_size == 0 || object_rx->data == NULL ||
	    object_rx->data_size == 0) {
		LOG_DBG("Error in output audio object");
		return -ECONNREFUSED;
	}

	ret = data_tx(NULL, hdl_tx, object_tx, NULL);
	if (ret) {
		LOG_DBG("Failed to send data to module %s, ret %d", hdl_tx->name, ret);
		return ret;
	}

	ret = data_fifo_pointer_last_filled_get(&hdl_rx->out_msg, (void **)&out_msg, &out_msg_size,
						timeout);
	if (ret) {
		LOG_DBG("Failed to retrieve data from module %s, ret %d", hdl_rx->name, ret);
		return ret;
	}

	if (out_msg->object.data == NULL || out_msg->object.data_size == 0) {
		LOG_DBG("Data output buffer too small for received buffer from module %s",
			hdl_rx->name);
		ret = -EINVAL;
	} else {
		uint8_t *data_out = object_rx->data;

		memcpy(object_rx, &out_msg->object, sizeof(struct aobj_object));
		memcpy(data_out, out_msg->object.data, out_msg->object.data_size);

		ret = 0;
	}

	object_release(handle_rx, &out_msg->object);

	data_fifo_block_free(&hdl_rx->out_msg, (void **)&out_msg);

	return ret;
};

/**
 * @brief Helper function to configure the thread information for the module
 *        set-up parameters structure.
 *
 */
int amod_parameters_configure(struct amod_parameters *parameters,
			      struct amod_description *description, k_thread_stack_t *stack,
			      size_t stack_size, int priority, int in_msg_num, int out_msg_num)
{
	if (parameters == NULL) {
		LOG_DBG("Error parameters pointer is NULL");
		return -EINVAL;
	}

	parameters->description = description;
	parameters->thread.stack = stack;
	parameters->thread.stack_size = stack_size;
	parameters->thread.priority = priority;
	parameters->thread.in_msg_num = in_msg_num;
	parameters->thread.out_msg_num = out_msg_num;

	return 0;
}

/**
 * @brief Helper function to return the base and instance names for a given
 *        module handle.
 *
 */
int amod_names_get(struct amod_handle *handle, char *base_name, char *instance_name)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL || base_name == NULL || instance_name == NULL) {
		LOG_DBG("Input parameter is NULL");
		return -EINVAL;
	}

	if (hdl->state >= AMOD_STATE_UNDEFINED) {
		LOG_DBG("Module %s is in an invalid state, %d, for get names", hdl->name,
			hdl->state);
		return -ENOTSUP;
	}

	memcpy(base_name, &hdl->description.name, sizeof(hdl->description.name));
	memcpy(instance_name, &hdl->name, sizeof(hdl->name));

	return 0;
}

/**
 * @brief Helper function to return the base and instance names for a given
 *        module handle.
 */
int amod_state_get(struct amod_handle *handle)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;

	if (handle == NULL) {
		LOG_DBG("Input parameter is NULL");
		return -EINVAL;
	}

	if (hdl->state >= AMOD_STATE_UNDEFINED) {
		LOG_DBG("Module %s is in an invalid state, %d, for get names", hdl->name,
			hdl->state);
		return -ENOTSUP;
	}

	return hdl->state;
};

/**
 * @brief Helper function to fill an audio data object.
 *
 */
int amod_object_fill(struct aobj_object *object, enum aobj_type data_type, char *data,
		     size_t data_size, struct aobj_format *format, struct aobj_sync *sync_data,
		     bool bad_frame, bool last_flag, void *user_data)
{
	if (object == NULL) {
		LOG_DBG("Input object parameter is NULL");
		return -EINVAL;
	}

	object->data_type = data_type;
	object->data = data;
	object->data_size = data_size;
	object->format = *format;
	object->sync_data = *sync_data;
	object->bad_frame = bad_frame;
	object->last_flag = last_flag;
	object->user_data = user_data;

	return 0;
};

/**
 * @brief Helper function to extract the audio data from an audio object.
 *
 */
int amod_object_data_extract(struct aobj_object *object, enum aobj_type *data_type, char *data,
			     size_t *data_size, struct aobj_format *format,
			     struct aobj_sync *sync_data, bool *bad_frame, bool *last_flag,
			     void *user_data)
{
	if (object == NULL) {
		LOG_DBG("Input object parameter is NULL");
		return -EINVAL;
	}

	*data_type = object->data_type;
	data = object->data;
	*data_size = object->data_size;
	*format = object->format;
	*sync_data = object->sync_data;
	*bad_frame = object->bad_frame;
	*last_flag = object->last_flag;
	user_data = object->user_data;

	return 0;
}
