/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "dummy.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "aobj_api.h"
#include "amod_api.h"
#include "amod_api_private.h"

#include "dummy_private.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dummy, 4); /* CONFIG_DUMMY_LOG_LEVEL); */

/**
 * @brief Number of micro seconds in a second.
 *
 */
#define LC3_DECODER_US_IN_A_SECOND (1000000)

/**
 * @brief Table of the dummy module functions.
 *
 */
const struct _amod_functions dummy_functions = {
	/**
	 * @brief  Function for querying the resources required for the dummy module.
	 */
	.query_resource = dummy_query_resource,

	/**
	 * @brief  Function to an open the dummy module.
	 */
	.open = dummy_open,

	/**
	 * @brief  Function to close the dummy module.
	 */
	.close = dummy_close,

	/**
	 * @brief  Function to set the configuration of the dummy module.
	 */
	.configuration_set = dummy_configuration_set,

	/**
	 * @brief  Function to get the configuration of the dummy module.
	 */
	.configuration_get = dummy_configuration_get,

	/**
	 * @brief Start a module processing data.
	 */
	.start = NULL,

	/**
	 * @brief Pause a module processing data.
	 */
	.pause = NULL,

	/**
	 * @brief The core data processing function in the dummy module.
	 */
	.data_process = dummy_data_process,
};

/**
 * @brief The set-up description for the LC3 decoder.
 *
 */
const struct amod_description dummy_dept = { .name = "Dummy Test Module",
					     .type = AMOD_TYPE_PROCESSOR,
					     .functions =
						     (struct amod_functions *)&dummy_functions };

/**
 * @brief A private pointer to the LC3 decoder set-up parameters.
 *
 */
const struct amod_description *dummy_description = &dummy_dept;

/**
 * @brief  Function for querying the resources required for the LC3 decoder
 *		   module with the given configuration.
 *
 */
int dummy_query_resource(struct amod_configuration *configuration)
{
	return sizeof(struct dummy_context);
}

/**
 * @brief Open an instance of the LC3 decoder
 *
 */
int dummy_open(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct _amod_handle *hdl = (struct _amod_handle *)handle;
	struct dummy_context *ctx = (struct dummy_context *)hdl->context;
	struct dummy_configuration *config = (struct dummy_configuration *)configuration;

	/* Perform any other functions required for the module */

	return 0;
}

/**
 * @brief  Function close an instance of the LC3 decoder.
 *
 */
int dummy_close(struct amod_handle *handle)
{
	struct _amod_handle *hdl = (struct _amod_handle *)handle;
	struct dummy_context *ctx = (struct dummy_context *)hdl->context;

	/* Perform any other functions required for the module */

	/* Clear context pointer */
	ctx = NULL;

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
int dummy_configuration_set(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct dummy_configuration *config = (struct dummy_configuration *)configuration;
	struct _amod_handle *hdl = (struct _amod_handle *)handle;
	struct dummy_context *ctx = (struct dummy_context *)hdl->context;

	/* Copy the configuration into the context */
	memcpy(&ctx->config, config, sizeof(struct dummy_configuration));

	/* Configure decoder */
	LOG_DBG("Dummy module %s configuration: rate = %d  depth = %  string = %s", hdl->name,
		ctx->config.rate, ctx->config.depth, ctx->config.some_string);

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the LC3 decoder.
 *
 */
int dummy_configuration_get(struct amod_handle *handle, struct amod_configuration *configuration)
{
	struct dummy_configuration *config = (struct dummy_configuration *)configuration;
	struct _amod_handle *hdl = (struct _amod_handle *)handle;
	struct dummy_context *ctx = (struct dummy_context *)hdl->context;

	/* Copy the configuration into the context */
	memcpy(config, &ctx->config, sizeof(struct dummy_configuration));

	/* Configure decoder */
	LOG_DBG("Dummy module %s configuration: rate = %d  depth = %  string = %s", hdl->name,
		config.rate, config.depth, config.some_string);

	return 0;
}

/**
 * @brief Process an audio data object in an instance of the LC3 decoder.
 *
 */
int dummy_data_process(struct amod_handle *handle, struct aobj_object *object_in,
		       struct aobj_object *object_out)
{
	struct _amod_handle *hdl = (struct _amod_handle *)handle;
	struct dummy_context *ctx = (struct dummy_context *)hdl->context;
	int i, j;
	int size = object_in->data_size;

	/* Test the object is of the right type, data sizes, etc.... */
	if (object_in->data_size > object_out->data_size) {
		LOG_INF("Can only copy %d from input as output buff is too small",
			object_in->data_size);
		size = object_out->data_size;
	}

	/* Reverse the input object data */
	data_in = &object_in->data[size - 1];
	data_out = object_out->data;
	for (i = 0; i < size; i++) {
		*data_out-- = *data_in++;
	}

	/* Save the last DUMMY_MODULE_LAST_BYTES_NUM bytes of the input buffer to the context */
	data_in = &object_in->data[object_in->data_size - 1];
	ctx_data = ctx->dummy_data;
	for (i = 0; i < DUMMY_MODULE_LAST_BYTES_NUM; i++) {
		*ctx_data++ = *data_in--;
	}

	return 0;
}
