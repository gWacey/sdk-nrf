/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX - License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "amod_api.h"
#include "aobj_api.h"
#include "data_fifo.h"
#include "lc3_decoder.h"
#include <ctype.h>
#include <nrfx_clock.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

/* Headers below only for testing */
#include "amod_api_private.h"
#include "lc3_decoder_private.h"

char *lc3_decoder_instance_name = "LC3 Decoder inst 1"

	char private_mem_static[WB_UP(MODULE_DATA_SIZE)];
char in_msg_mem_static[AMOD_IN_MSG_SET_SIZE(LC3_DECODER_IN_MESSAGE_NUM)];
char out_msg_mem_static[AMOD_OUT_MSG_SET_SIZE(LC3_DECODER_OUT_MESSAGE_NUM)];
char data_mem_static[AMOD_DATA_BUF_SET_SIZE(WB_UP(FRAME_SIZE_BYTES), LC3_DECODER_CHANNELS,
					    LC3_DECODER_DATA_NUM)];

K_THREAD_STACK_DEFINE(lc3_decoder_thread_stack, LC3_DECODER_THREAD_STACK_SIZE);

struct lc3_decoder_configuration lc3_dec_inst_1_config =
	= { .sample_rate = 48000,
	    .bit_depth = 16,
	    .duration_us = 10000,
	    .number_channels = LC3_DECODER_CHANNELS,
	    .channel_map = (LEFT_CHANNEL || RIGHT_CHANNEL) };

/**
 * @brief Static allocate memory for the LC3 decoder module.
 *
 */
static int static_allocate_memory(amod_handle *handle, char **in_msg_mem, char **out_msg_mem,
				  char **data_mem)
{
	*handle = &private_mem_static[0];
	*in_msg_mem = &in_msg_mem_static[0];
	*out_msg_mem = &out_msg_mem_static[0];
	*data_mem = &data_mem_static[0];

	return 0;
}

/**
 * @brief Dynamic allocate memory for the LC3 decoder module.
 *
 */
static int dynamic_allocate_memory(amod_parameters *parameters, amod_configuration mod_config,
				   amod_handle *handle, char **in_msg_mem, char **out_msg_mem,
				   char **data_mem)
{
	int ret;
	int handle_size, in_msg_size, out_msg_size, data_size;
	struct _amod_parameters *params = (struct _amod_parameters *)parameters;

	handle_size = amod_query_resource(params, mod_config);
	if (handle_size < 0) {
		printf("Querry resource FAILED , ret %d", handle_size);
		return -1;
	}

	*handle = k_alloc(handle_size, K_NO_WAIT);

	if (params->thread.in_msg_num > 0) {
		*in_msg_mem = k_heap_calloc(params->thread.in_msg_num, AMOD_IN_MSG_SIZE);
	} else {
		*in_msg_mem = NULL;
	}

	if (params->thread.out_msg_num > 0) {
		*out_msg_mem = k_heap_calloc(params->thread.out_msg_num, AMOD_OUT_MSG_SIZE);
	} else {
		*out_msg_mem = NULL;
	}

	if (params->thread.out_msg_num > 0) {
		*data_mem = k_heap_calloc(params->thread.data_num, params->thread.data_size);
	} else {
		*data_mem = NULL;
	}

	return 0;
}

/**
 * @brief Basic test of a single module
 *
 */
static int basic_test(char *in_msg, char *out_msg, char *data, amod_handle handle)
{
	char *base_name;
	char *instance_name;
	struct lc3_decoder_configuration lc3_dec_config_retrieved;

	ret = amod_open(lc3_dec_parameters, lc3_decoder_instance_name, in_msg, out_msg, data,
			handle);
	if (ret) {
		printf("Open FAILED, ret %d", ret);
		return -1;
	}

	ret = amod_names_get(handle, base_name, instance_name);
	if (ret) {
		printf("Failed to get names for module, ret %d", ret);
		return -1;
	}

	if (memcmp(base_name, lc3_dec_parameters->name, strlen(lc3_dec_parameters->name))) {
		printf("Failed to read the base name correctly (%s) for module %s", base_name,
		       hdl->name);
		return -1;
	}

	if (memcmp(instance_name, lc3_dec_parameters->name, strlen(lc3_dec_parameters->name))) {
		printf("Failed to read the instance name correctly (%s) for module %s",
		       instance_name, hdl->name);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AUDIO_MODULE_STATE_OPEN) {
		printf("Failed to get the correct state for module %s instance %s, ret %d",
		       base_name, instance_name, ret);
		return -1;
	}

	ret = amod_configuration_set(handle, lc3_dec_inst_1_config);
	if (ret) {
		printf("Set configuration FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AUDIO_MODULE_STATE_CONFIGURED) {
		printf("Failed to get the correct state for module %s instance %s, ret %d",
		       base_name, instance_name, ret);
		return -1;
	}

	ret = amod_configuration_get(handle, lc3_dec_config_retrieved);
	if (ret != -ENOSYS) {
		printf("Get configuration FAILED, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AUDIO_MODULE_STATE_RUNNING) {
		printf("Failed to get the correct state for module %s instance %s, ret %d",
		       base_name, instance_name, ret);
		return -1;
	}

	ret = amod_start(handle);
	if (ret) {
		printf("Start FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_RUNNING) {
		printf("Failed to get the correct state for module %s instance %s, ret %d",
		       base_name, instance_name, ret);
		return -1;
	}

	ret = amod_pause(handle);
	if (ret) {
		printf("Send command FAILED test, ret %d", ret);
		return -1;
	}

	state = amod_state_get(handle);
	if (state != AMOD_STATE_PAUSED) {
		printf("Failed to get the correct state for module %s instance %s, ret %d",
		       base_name, instance_name, ret);
		return -1;
	}

	return 0;
}

/**
 * @brief Function to start a single LC3 decoder module and configure it.
 *
 */
static int lc3_decoder_static_test(void)
{
	int ret;
	amod_handle handle;
	char *in_msg;
	char *out_msg;
	char *data;

	amod_thread_configure(lc3_dec_parameters.thread_config, lc3_decoder_thread_stack,
			      LC3_DECODER_THREAD_STACK_SIZE, LC3_DECODER_THREAD_PRIORITY,
			      LC3_DECODER_IN_MESSAGE_NUM, LC3_DECODER_OUT_MESSAGE_NUM,
			      LC3_DECODER_DATA_NUM);
	if (ret) {
		printf("Thread configuration FAILED , ret %d", ret);
		return -1;
	}

	ret = static_allocate_memory(&handle, &in_msg, &out_msg, &data);
	if (ret) {
		printf("Failed to allocate memory, ret %d", ret);
		return -1;
	}

	ret = basic_test(in_msg, out_msg, data, handle);
	if (ret) {
		printf("Failed to basic tests, ret %d", ret);
		return -1;
	}

	ret = amod_close(&handle);
	if (ret) {
		printf("Close FAILED, ret %d", ret);
		return -1;
	}

	if (handle != NULL) {
		printf("Close failed to NULL handle");
		return -1;
	}

	return 0;
}

/**
 * @brief Function to start a single module to the configured stage.
 *
 */
static int void lc3_decoder_dynamic_test(void)
{
	int ret;
	amod_handle handle;
	amod_handle handle_memory;
	char *in_msg;
	char *out_msg;
	char *data;

	amod_thread_configure(lc3_dec_parameters.thread_config, lc3_decoder_thread_stack,
			      LC3_DECODER_THREAD_STACK_SIZE, LC3_DECODER_THREAD_PRIORITY,
			      LC3_DECODER_IN_MESSAGE_NUM, LC3_DECODER_OUT_MESSAGE_NUM,
			      LC3_DECODER_DATA_NUM);
	if (ret) {
		printf("Thread configuration FAILED , ret %d", ret);
		return -1;
	}

	ret = dynamic_allocate_memory(lc3_dec_parameters, &lc3_dec_inst_1_config, &handle, &in_msg,
				      &out_msg, &data);
	if (ret) {
		printf("Failed to allocate memory, ret %d", ret);
		return -1;
	}

	ret = basic_test(in_msg, out_msg, data, handle);
	if (ret) {
		printf("Failed to basic tests, ret %d", ret);
		return -1;
	}

	handle_memory = handle;

	ret = amod_close(&handle);
	if (ret) {
		printf("Close FAILED, ret %d", ret);
		return -1;
	}

	if (handle != NULL) {
		printf("Close failed to NULL handle");
		return -1;
	}

	k_free(in_msg);
	k_free(out_msg);
	k_free(data);
	k_free(handle);

	return 0;
}

void main(void)
{
	int ret;

	ret = lc3_decoder_static_test();
	if (ret) {
		printf("Failed: LC3 decoder static memory test");
	}
	printf("Passed: LC3 decoder static memory test");

	ret = lc3_decoder_dynamic_test();
	if (ret) {
		printf("Failed: LC3 decoder dynamic memory test");
	}
	printf("Passed: LC3 decoder dynamic memory test");
}
