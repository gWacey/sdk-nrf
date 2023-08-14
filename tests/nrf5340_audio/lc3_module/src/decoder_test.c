/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/tc_util.h>

#include "ablk_api.h"
#include "amod_api.h"
#include "audio_data.h"
#include "lc3_decoder.h"

#define TEST_ENC_MONO_DATA_SIZE	  (sizeof(encoded_sine_100hz_mono) / sizeof(int16_t))
#define TEST_ENC_STEREO_DATA_SIZE (sizeof(encoded_sine_100hz_stereo) / sizeof(int16_t))
#define TEST_DEC_MONO_DATA_SIZE	  (sizeof(decoded_sine_100hz_mono) / sizeof(int16_t))
#define TEST_DEC_STEREO_DATA_SIZE (sizeof(decoded_sine_100hz_stereo) / sizeof(int16_t))
#define TEST_ENC_MONO_BUF_SIZE	  120
#define TEST_ENC_STEREO_BUF_SIZE  (TEST_ENC_MONO_BUF_SIZE * 2)
#define TEST_DEC_MONO_BUF_SIZE	  960
#define TEST_DEC_STEREO_BUF_SIZE  (TEST_DEC_MONO_BUF_SIZE * 2)
#define TEST_PCM_SAMPLE_RATE	  48000
#define TEST_PCM_BIT_DEPTH	  16
#define TEST_LC3_BITRATE	  96000
#define TEST_LC3_FRAME_SIZE_US	  10000
#define TEST_LC3_NUM_CHANNELS	  2
#define TEST_AUDIO_CH_L		  0
#define TEST_AUDIO_CH_R		  1

#define CONFIG_DECODER_MSG_QUEUE_SIZE	   (4)
#define CONFIG_DATA_OBJECTS_NUM		   (4)
#define CONFIG_LC3_DECODER_STACK_SIZE	   (4096)
#define CONFIG_LC3_DECODER_THREAD_PRIORITY (4)
#define CONFIG_MODULES_NUM		   (3)
#define CONFIG_DECODER_MODULES_NUM	   (2)
#define CONFIG_ENCODER_MODULES_NUM	   (1)
#define CONFIG_DECODER_BUFFER_NUM	   (8)

enum test_module_id {
	TEST_MODULE_LC3_DECODER = 0,
	TEST_MODULE_LC3_ENCODER,
	TEST_MODULES_NUM
};

enum test_lc3_decoder_module_id {
	TEST_MODULE_ID_DECODER_1 = 0,
	TEST_MODULE_ID_DECODER_2,
	TEST_MODULE_ID_DECODER_NUM
};

struct amod_handle handle[CONFIG_DECODER_MODULES_NUM + CONFIG_ENCODER_MODULES_NUM];
struct lc3_decoder_context decoder_ctx[CONFIG_DECODER_MODULES_NUM];
K_THREAD_STACK_DEFINE(lc3_dec1_thread_stack, CONFIG_LC3_DECODER_STACK_SIZE);
DATA_FIFO_DEFINE(lc3_dec1_fifo_tx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(lc3_dec1_fifo_rx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));

K_THREAD_STACK_DEFINE(lc3_dec2_thread_stack, CONFIG_LC3_DECODER_STACK_SIZE);
DATA_FIFO_DEFINE(lc3_dec2_fifo_tx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(lc3_dec2_fifo_rx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));

K_MEM_SLAB_DEFINE(audio_data_slab, TEST_DEC_MONO_BUF_SIZE, CONFIG_DATA_OBJECTS_NUM,
		  CONFIG_DECODER_BUFFER_NUM);

struct ablk_block test_block = {.data_type = ABLK_TYPE_LC3,
				.frame_len_us = TEST_LC3_FRAME_SIZE_US,
				.sample_rate_hz = TEST_PCM_SAMPLE_RATE,
				.bits_per_sample = TEST_PCM_BIT_DEPTH,
				.bitrate = TEST_LC3_BITRATE,
				.carrier_size = TEST_PCM_BIT_DEPTH,
				.interleaved = ABLK_DEINTERLEAVED,
				.reference_ts_us = 0,
				.data_rx_ts_us = 0,
				.bad_frame = false,
				.user_data = NULL,
				.user_data_size = 0};

static void test_lc3_decoder_mono_init(void)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bit_depth = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L);

	data_fifo_init(&lc3_dec1_fifo_rx);
	data_fifo_init(&lc3_dec1_fifo_tx);

	test_decoder_param.description = lc3_decoder_description;
	test_decoder_param.thread.stack_size = CONFIG_LC3_DECODER_STACK_SIZE;
	test_decoder_param.thread.priority = CONFIG_LC3_DECODER_THREAD_PRIORITY;
	test_decoder_param.thread.data_slab = &audio_data_slab;
	test_decoder_param.thread.data_size = TEST_DEC_MONO_BUF_SIZE;
	test_decoder_param.thread.stack = lc3_dec1_thread_stack,
	test_decoder_param.thread.msg_rx = &lc3_dec1_fifo_rx;
	test_decoder_param.thread.msg_tx = &lc3_dec1_fifo_tx;

	ret = amod_open(&test_decoder_param, (struct amod_configuration *)&test_decoder_config,
			"Decoder 1", (struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module open did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module start did not return zero");
}

static void test_lc3_decoder_dual_mono_init(void)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bit_depth = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L);

	data_fifo_init(&lc3_dec1_fifo_rx);
	data_fifo_init(&lc3_dec1_fifo_tx);

	test_decoder_param.description = lc3_decoder_description;
	test_decoder_param.thread.stack_size = CONFIG_LC3_DECODER_STACK_SIZE;
	test_decoder_param.thread.priority = CONFIG_LC3_DECODER_THREAD_PRIORITY;
	test_decoder_param.thread.data_slab = &audio_data_slab;
	test_decoder_param.thread.data_size = TEST_DEC_MONO_BUF_SIZE;
	test_decoder_param.thread.stack = lc3_dec1_thread_stack,
	test_decoder_param.thread.msg_rx = &lc3_dec1_fifo_rx;
	test_decoder_param.thread.msg_tx = &lc3_dec1_fifo_tx;

	ret = amod_open(&test_decoder_param, (struct amod_configuration *)&test_decoder_config,
			"Decoder 1", (struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module open did not return zero");

	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_R);

	data_fifo_init(&lc3_dec2_fifo_rx);
	data_fifo_init(&lc3_dec2_fifo_tx);

	test_decoder_param.description = lc3_decoder_description;
	test_decoder_param.thread.stack_size = CONFIG_LC3_DECODER_STACK_SIZE;
	test_decoder_param.thread.priority = CONFIG_LC3_DECODER_THREAD_PRIORITY;
	test_decoder_param.thread.data_slab = &audio_data_slab;
	test_decoder_param.thread.data_size = TEST_DEC_MONO_BUF_SIZE;
	test_decoder_param.thread.stack = lc3_dec2_thread_stack,
	test_decoder_param.thread.msg_rx = &lc3_dec2_fifo_rx;
	test_decoder_param.thread.msg_tx = &lc3_dec2_fifo_tx;

	ret = amod_open(&test_decoder_param, (struct amod_configuration *)&test_decoder_config,
			"Decoder 2", (struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_2],
			&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Decoder right module open did not return zero");

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_1], &handle[TEST_MODULE_ID_DECODER_1]);
	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_2], &handle[TEST_MODULE_ID_DECODER_2]);

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module start did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Decoder right module start did not return zero");
}

static void test_lc3_decoder_stereo_init(void)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bit_depth = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L) | BIT(TEST_AUDIO_CH_R);

	data_fifo_init(&lc3_dec1_fifo_rx);
	data_fifo_init(&lc3_dec1_fifo_tx);

	test_decoder_param.description = lc3_decoder_description;
	test_decoder_param.thread.stack_size = CONFIG_LC3_DECODER_STACK_SIZE;
	test_decoder_param.thread.priority = CONFIG_LC3_DECODER_THREAD_PRIORITY;
	test_decoder_param.thread.data_slab = &audio_data_slab;
	test_decoder_param.thread.data_size = TEST_DEC_MONO_BUF_SIZE;
	test_decoder_param.thread.stack = lc3_dec1_thread_stack,
	test_decoder_param.thread.msg_rx = &lc3_dec1_fifo_rx;
	test_decoder_param.thread.msg_tx = &lc3_dec1_fifo_tx;

	ret = amod_open(&test_decoder_param, (struct amod_configuration *)&test_decoder_config,
			"Decoder 1", (struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module open did not return zero");

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_1], &handle[TEST_MODULE_ID_DECODER_1]);

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module start did not return zero");
}

ZTEST(suite_lc3_decoder, test_lc3_decode_mono)
{
	int ret;
	int16_t *input_data = (int16_t *)&encoded_sine_100hz_mono[0];
	int16_t *input_end = (int16_t *)&encoded_sine_100hz_mono[TEST_ENC_MONO_DATA_SIZE];
	int16_t *ref_data = (int16_t *)&decoded_sine_100hz_mono[0];
	int16_t *ref_end = (int16_t *)&decoded_sine_100hz_mono[TEST_DEC_MONO_DATA_SIZE];
	int16_t data[TEST_DEC_MONO_BUF_SIZE];
	struct ablk_block block_tx, block_rx;

	test_lc3_decoder_mono_init();

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = (void *)input_data;
		block_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_rx.data_type = ABLK_TYPE_PCM;
		block_rx.data = (void *)data;
		block_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for right decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&data[0], ref_data, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");

		input_data += TEST_ENC_MONO_BUF_SIZE;
		ref_data += TEST_DEC_MONO_BUF_SIZE;

	} while (input_data < input_end || ref_data < ref_end);
}

ZTEST(suite_lc3_decoder, test_lc3_decode_dual_mono)
{
	int ret;
	int16_t *input_data = (int16_t *)&encoded_sine_100hz_mono[0];
	int16_t *input_end = (int16_t *)&encoded_sine_100hz_mono[TEST_ENC_MONO_DATA_SIZE];
	int16_t *ref_data = (int16_t *)&decoded_sine_100hz_mono[0];
	int16_t *ref_end = (int16_t *)&decoded_sine_100hz_mono[TEST_DEC_MONO_DATA_SIZE];
	uint8_t data_l[TEST_DEC_MONO_BUF_SIZE], data_r[TEST_DEC_MONO_BUF_SIZE];
	struct ablk_block block_tx, block_rx;

	test_lc3_decoder_dual_mono_init();

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = (void *)input_data;
		block_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_rx.data = (void *)data_l;
		block_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes for right incorrect: %d");
		zassert_mem_equal(&data_l[0], ref_data, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded right PCM data does not match reference PCM data");

		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = (void *)input_data;
		block_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_rx.data = (void *)data_r;
		block_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_2],
				      &handle[TEST_MODULE_ID_DECODER_2], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for right decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes for left incorrect: %d");
		zassert_mem_equal(&data_r[0], ref_data, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded left PCM data does not match reference PCM data");
		zassert_mem_equal(&data_l[0], &data_r[0], TEST_DEC_MONO_BUF_SIZE,
				  "Left PCM data does not match right PCM data");

		input_data += TEST_ENC_MONO_BUF_SIZE;
		ref_data += TEST_DEC_MONO_BUF_SIZE;

	} while (input_data < input_end || ref_data < ref_end);
}

ZTEST(suite_lc3_decoder, test_lc3_decode_stereo_deint)
{
	int ret;
	int16_t *input_data = (int16_t *)&encoded_sine_100hz_stereo[0];
	int16_t *input_end = (int16_t *)&encoded_sine_100hz_stereo[TEST_ENC_STEREO_DATA_SIZE];
	int16_t *ref_data = (int16_t *)&decoded_sine_100hz_stereo[0];
	int16_t *ref_end = (int16_t *)&decoded_sine_100hz_stereo[TEST_DEC_STEREO_DATA_SIZE];
	int16_t data[TEST_DEC_STEREO_BUF_SIZE];
	struct ablk_block block_tx, block_rx;

	test_lc3_decoder_stereo_init();

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = input_data;
		block_tx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		block_rx.data_type = ABLK_TYPE_PCM;
		block_rx.data = (void *)data;
		block_rx.data_size = TEST_DEC_STEREO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_STEREO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&data[0], ref_data, TEST_DEC_STEREO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");

		input_data += TEST_ENC_STEREO_BUF_SIZE;
		ref_data += TEST_DEC_STEREO_BUF_SIZE;

	} while (input_data < input_end || ref_data < ref_end);
}

ZTEST(suite_lc3_decoder, test_lc3_decode_stereo_int)
{
	int ret;
	int16_t *input_data = (int16_t *)&encoded_sine_100hz_stereo[0];
	int16_t *input_end = (int16_t *)&encoded_sine_100hz_stereo[TEST_ENC_STEREO_DATA_SIZE];
	int16_t *ref_data = (int16_t *)&decoded_sine_100hz_stereo[0];
	int16_t *ref_end = (int16_t *)&decoded_sine_100hz_stereo[TEST_DEC_STEREO_DATA_SIZE];
	int16_t data[TEST_DEC_STEREO_BUF_SIZE];
	struct ablk_block block_tx, block_rx;

	test_lc3_decoder_stereo_init();

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = input_data;
		block_tx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		block_rx.data_type = ABLK_TYPE_PCM;
		block_rx.data = data;
		block_rx.data_size = TEST_DEC_STEREO_BUF_SIZE;
		block_rx.interleaved = ABLK_INTERLEAVED;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_STEREO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&data[0], ref_data, TEST_DEC_STEREO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");

		input_data += TEST_ENC_STEREO_BUF_SIZE;
		ref_data += TEST_DEC_STEREO_BUF_SIZE;

	} while (input_data < input_end || ref_data < ref_end);
}
