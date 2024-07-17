/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/debug/thread_analyzer.h>
#include <errno.h>

#include "lc3_test_fakes.h"
#include "audio_module.h"
#include "lc3_test_common.h"
#include "lc3_encoder.h"
#include "sweep21ms_16b48khz_mono_lc3.h"
#include "sweep21ms_16b48khz_mono_wav.h"

#define TEST_LC3_ENCODER_MSG_QUEUE_SIZE	      (4)
#define TEST_LC3_ENCODER_DATA_OBJECTS_NUM     (2)
#define TEST_LC3_ENCODER_STACK_SIZE	      (4192)
#define TEST_LC3_ENCODER_THREAD_PRIORITY      (4)
#define TEST_LC3_ENCODER_MODULES_NUM	      (TEST_AUDIO_CHANNELS_MAX)
#define TEST_LC3_ENCODER_MSG_SIZE	      (sizeof(struct audio_module_message))
#define TEST_LC3_ENCODER_AUDIO_DATA_ITEMS_NUM (4)

enum test_lc3_encoder_module_id {
	TEST_MODULE_ID_ENCODER_1 = 0,
	TEST_MODULE_ID_ENCODER_2,
	TEST_MODULE_ID_ENCODER_NUM
};

static struct audio_module_handle handle[TEST_LC3_ENCODER_MODULES_NUM];
static struct lc3_encoder_context lc3_enc_ctx[TEST_LC3_ENCODER_MODULES_NUM];

K_THREAD_STACK_ARRAY_DEFINE(lc3_enc_thread_stack, TEST_LC3_ENCODER_MODULES_NUM,
			    TEST_LC3_ENCODER_STACK_SIZE);
DATA_FIFO_DEFINE(lc3_enc_msg_fifo_tx, TEST_LC3_ENCODER_MSG_QUEUE_SIZE, TEST_LC3_ENCODER_MSG_SIZE);
DATA_FIFO_DEFINE(lc3_enc_msg_fifo_rx, TEST_LC3_ENCODER_MSG_QUEUE_SIZE, TEST_LC3_ENCODER_MSG_SIZE);
DATA_FIFO_DEFINE(lc3_enc_msg_fifo_tx1, TEST_LC3_ENCODER_MSG_QUEUE_SIZE, TEST_LC3_ENCODER_MSG_SIZE);
DATA_FIFO_DEFINE(lc3_enc_msg_fifo_rx1, TEST_LC3_ENCODER_MSG_QUEUE_SIZE, TEST_LC3_ENCODER_MSG_SIZE);

static struct data_fifo *msg_fifo_tx_array[] = {&lc3_enc_msg_fifo_tx, &lc3_enc_msg_fifo_tx1};
static struct data_fifo *msg_fifo_rx_array[] = {&lc3_enc_msg_fifo_rx, &lc3_enc_msg_fifo_rx1};

static char audio_data_memory[TEST_ENC_MULTI_BUF_SIZE * TEST_LC3_ENCODER_DATA_OBJECTS_NUM];
static struct k_mem_slab audio_data_slab;

static struct audio_data audio_data = {.meta = {.data_coding = PCM,
						.data_len_us = TEST_LC3_FRAME_SIZE_US,
						.sample_rate_hz = TEST_PCM_SAMPLE_RATE,
						.bits_per_sample = TEST_PCM_BIT_DEPTH,
						.carried_bits_per_sample = TEST_PCM_BIT_DEPTH,
						.reference_ts_us = 0,
						.data_rx_ts_us = 0,
						.bad_data = false}};

static void test_lc3_encoder_mono_multi_init(uint32_t locations)
{
	int ret;
	struct audio_module_parameters test_encoder_param;
	struct lc3_encoder_configuration test_encoder_config;
	uint8_t number_channels;

	for (int i = 0; i < TEST_LC3_ENCODER_MODULES_NUM; i++) {
		memset(&handle[i], 0, sizeof(struct audio_module_handle));
	}

	audio_module_number_channels_calculate(locations, &number_channels);

	ret = k_mem_slab_init(&audio_data_slab, &audio_data_memory[0],
			      TEST_ENC_MONO_BUF_SIZE * number_channels,
			      TEST_LC3_ENCODER_DATA_OBJECTS_NUM);
	zassert_equal(ret, 0, "Failed to allocate the data slab: ret = %d", ret);

	test_encoder_config.sample_rate_hz = TEST_PCM_SAMPLE_RATE;
	test_encoder_config.bits_per_sample = TEST_SAMPLE_BIT_DEPTH;
	test_encoder_config.carried_bits_per_sample = TEST_PCM_BIT_DEPTH;
	test_encoder_config.data_len_us = TEST_LC3_FRAME_SIZE_US;
	test_encoder_config.interleaved = false;
	test_encoder_config.bitrate_bps_max = TEST_LC3_BITRATE;

	for (int i = 0; i < number_channels; i++) {
		test_encoder_config.locations = 1 << i;

		test_encoder_param.description = lc3_encoder_description;
		test_encoder_param.thread.stack_size = TEST_LC3_ENCODER_STACK_SIZE;
		test_encoder_param.thread.priority = TEST_LC3_ENCODER_THREAD_PRIORITY;
		test_encoder_param.thread.data_slab = &audio_data_slab;
		test_encoder_param.thread.data_size = TEST_ENC_MONO_BUF_SIZE;
		test_encoder_param.thread.stack = lc3_enc_thread_stack[i],
		test_encoder_param.thread.msg_rx = msg_fifo_rx_array[i];
		test_encoder_param.thread.msg_tx = msg_fifo_tx_array[i];

		ret = audio_module_open(&test_encoder_param,
					(struct audio_module_configuration *)&test_encoder_config,
					"Enc Multi-channel",
					(struct audio_module_context *)&lc3_enc_ctx[i], &handle[i]);
		zassert_equal(ret, 0, "Encoder left module open did not return zero: %d", ret);
		zassert_equal(LC3Initialize_fake.call_count, 1,
			      "Failed to call LC3 initialize %d times",
			      LC3Initialize_fake.call_count);
		zassert_equal(LC3EncodeSessionClose_fake.call_count, 0,
			      "Called LC3 close encoder session %d times",
			      LC3EncodeSessionClose_fake.call_count);
		zassert_equal(LC3BitstreamBuffersize_fake.call_count, (i + 1),
			      "Failed to call LC3 get buffer size %d times",
			      LC3BitstreamBuffersize_fake.call_count);
		zassert_equal(LC3EncodeSessionOpen_fake.call_count, (i + 1),
			      "Failed to call LC3 open encoder session %d times",
			      LC3EncodeSessionOpen_fake.call_count);

		ret = audio_module_connect(&handle[i], NULL, true);
		zassert_equal(ret, 0, "Encoder connect did not return zero");

		ret = audio_module_start(&handle[i]);
		zassert_equal(ret, 0, "Encoder left module start did not return zero");
	}
}

static void test_lc3_encoder_multi_init(bool pcm_format, uint32_t locations)
{
	int ret;
	struct audio_module_parameters test_encoder_param;
	struct lc3_encoder_configuration test_encoder_config;
	uint8_t number_channels;

	for (int i = 0; i < TEST_LC3_ENCODER_MODULES_NUM; i++) {
		memset(&handle[i], 0, sizeof(struct audio_module_handle));
	}

	audio_module_number_channels_calculate(locations, &number_channels);

	ret = k_mem_slab_init(&audio_data_slab, &audio_data_memory[0],
			      TEST_ENC_MONO_BUF_SIZE * number_channels,
			      TEST_LC3_ENCODER_DATA_OBJECTS_NUM);
	zassert_equal(ret, 0, "Failed to allocate the data slab: ret = %d", ret);

	test_encoder_config.sample_rate_hz = TEST_PCM_SAMPLE_RATE;
	test_encoder_config.bits_per_sample = TEST_SAMPLE_BIT_DEPTH;
	test_encoder_config.carried_bits_per_sample = TEST_PCM_BIT_DEPTH;
	test_encoder_config.data_len_us = TEST_LC3_FRAME_SIZE_US;
	test_encoder_config.interleaved = pcm_format;
	test_encoder_config.locations = locations;
	test_encoder_config.bitrate_bps_max = TEST_LC3_BITRATE;

	test_encoder_param.description = lc3_encoder_description;
	test_encoder_param.thread.stack_size = TEST_LC3_ENCODER_STACK_SIZE;
	test_encoder_param.thread.priority = TEST_LC3_ENCODER_THREAD_PRIORITY;
	test_encoder_param.thread.data_slab = &audio_data_slab;
	test_encoder_param.thread.data_size = TEST_ENC_MONO_BUF_SIZE * number_channels;
	test_encoder_param.thread.stack = lc3_enc_thread_stack[0],
	test_encoder_param.thread.msg_rx = msg_fifo_rx_array[0];
	test_encoder_param.thread.msg_tx = msg_fifo_tx_array[0];

	ret = audio_module_open(
		&test_encoder_param, (struct audio_module_configuration *)&test_encoder_config,
		"Enc Multi-channel", (struct audio_module_context *)&lc3_enc_ctx[0], &handle[0]);
	zassert_equal(ret, 0, "Encoder left module open did not return zero");
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);
	zassert_equal(LC3EncodeSessionClose_fake.call_count, 0,
		      "Called LC3 close encoder session %d times",
		      LC3EncodeSessionClose_fake.call_count);
	zassert_equal(LC3BitstreamBuffersize_fake.call_count, 1,
		      "Failed to call LC3 get buffer size %d times",
		      LC3BitstreamBuffersize_fake.call_count);
	zassert_equal(LC3EncodeSessionOpen_fake.call_count, number_channels,
		      "Failed to call LC3 open encoder session %d times",
		      LC3EncodeSessionOpen_fake.call_count);

	ret = audio_module_connect(&handle[0], NULL, true);
	zassert_equal(ret, 0, "Encoder connect did not return zero");

	ret = audio_module_start(&handle[0]);
	zassert_equal(ret, 0, "Encoder left module start did not return zero");
}

ZTEST(suite_lc3_encoder_functional, test_lc3_encode_mono)
{
	int ret;
	int enc_call_count = 0;
	uint8_t *data_in = (uint8_t *)&wav_mono[0];
	uint8_t *data_ref = (uint8_t *)&lc3_mono[0];
	uint8_t pcm_out[TEST_ENC_MONO_BUF_SIZE];
	struct audio_data audio_data_tx, audio_data_rx;

	/* Fake internal empty data FIFO success */
	ref_in = (uint8_t *)&lc3_mono[0];
	ref_in_size = sizeof(lc3_mono);
	ref_in_read_size = TEST_ENC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3EncodeSessionClose_fake.custom_fake = fake_LC3EncodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3EncodeSessionOpen_fake.custom_fake = fake_LC3EncodeSessionOpen__succeeds;
	LC3EncodeSessionData_fake.custom_fake = fake_LC3EncodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed, called LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_encoder_mono_multi_init(TEST_AUDIO_MONO_LEFT_LOCATIONS);

	do {
		memcpy(&audio_data_tx, &audio_data, sizeof(struct audio_data));
		memcpy(&audio_data_rx, &audio_data, sizeof(struct audio_data));

		audio_data_tx.data = (void *)data_in;
		audio_data_tx.data_size = TEST_DEC_MONO_BUF_SIZE;
		audio_data_tx.meta.data_coding = PCM;
		audio_data_tx.meta.locations = TEST_AUDIO_MONO_LEFT_LOCATIONS;

		audio_data_rx.data = pcm_out;
		audio_data_rx.data_size = TEST_ENC_MONO_BUF_SIZE;
		audio_data_rx.meta.data_coding = LC3;

		ret = audio_module_data_tx_rx(&handle[TEST_MODULE_ID_ENCODER_1],
					      &handle[TEST_MODULE_ID_ENCODER_1], &audio_data_tx,
					      &audio_data_rx, K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for the encoder: %d", ret);
		zassert_equal(audio_data_rx.data_size, TEST_ENC_MONO_BUF_SIZE,
			      "Encoder number of bytes incorrect: %d", audio_data_rx.data_size);
		zassert_mem_equal(&pcm_out[0], data_ref, TEST_ENC_MONO_BUF_SIZE,
				  "Encoder data does not match reference coded data");
		zassert_equal(LC3EncodeSessionData_fake.call_count, enc_call_count + 1,
			      "Failed to call LC3 encoder %d times",
			      LC3EncodeSessionData_fake.call_count);

		data_in += TEST_DEC_MONO_BUF_SIZE;
		data_ref += TEST_ENC_MONO_BUF_SIZE;

		enc_call_count += 1;

	} while ((enc_call_count < (LC3_MONO_SIZE / TEST_DEC_MONO_BUF_SIZE) &&
		  enc_call_count < (WAV_MONO_SIZE / TEST_ENC_MONO_BUF_SIZE)));

	ret = audio_module_stop(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = audio_module_close(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed, called LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

ZTEST(suite_lc3_encoder_functional, test_lc3_encode_dual_mono)
{
	int ret;
	int enc_call_count = 0;
	uint8_t *data_l_in = (uint8_t *)&wav_mono[0];
	uint8_t *data_r_in = (uint8_t *)&wav_mono[0];
	uint8_t *data_l_ref = (uint8_t *)&lc3_mono[0];
	uint8_t *data_r_ref = (uint8_t *)&lc3_mono[0];
	uint8_t data_l[TEST_ENC_MONO_BUF_SIZE], data_r[TEST_ENC_MONO_BUF_SIZE];
	struct audio_data audio_data_l_tx, audio_data_l_rx;
	struct audio_data audio_data_r_tx, audio_data_r_rx;

	/* Fake internal empty data FIFO success */
	ref_in = (uint8_t *)&lc3_mono[0];
	ref_in_size = sizeof(lc3_mono);
	ref_in_read_size = TEST_ENC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3EncodeSessionClose_fake.custom_fake = fake_LC3EncodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3EncodeSessionOpen_fake.custom_fake = fake_LC3EncodeSessionOpen__succeeds;
	LC3EncodeSessionData_fake.custom_fake = fake_LC3EncodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed, called LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_encoder_mono_multi_init(TEST_AUDIO_MONO_LEFT_LOCATIONS +
					 TEST_AUDIO_MONO_RIGHT_LOCATIONS);

	do {
		memcpy(&audio_data_l_tx, &audio_data, sizeof(struct audio_data));
		memcpy(&audio_data_l_rx, &audio_data, sizeof(struct audio_data));

		audio_data_l_tx.data = (void *)data_l_in;
		audio_data_l_tx.data_size = TEST_DEC_MONO_BUF_SIZE;
		audio_data_l_tx.meta.data_coding = PCM;
		audio_data_l_tx.meta.locations = TEST_AUDIO_MONO_LEFT_LOCATIONS;

		memset(&data_l[0], 0, TEST_ENC_MONO_BUF_SIZE);
		audio_data_l_rx.data = (void *)&data_l[0];
		audio_data_l_rx.data_size = TEST_ENC_MONO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle[TEST_MODULE_ID_ENCODER_1],
					      &handle[TEST_MODULE_ID_ENCODER_1], &audio_data_l_tx,
					      &audio_data_l_rx, K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left encoder: %d", ret);
		zassert_equal(audio_data_l_rx.data_size, TEST_ENC_MONO_BUF_SIZE,
			      "Encoder number of bytes for right incorrect: %d",
			      audio_data_l_rx.data_size);
		zassert_mem_equal(&data_l[0], data_l_ref, TEST_ENC_MONO_BUF_SIZE,
				  "Encoder left data does not match reference coded data");
		zassert_equal(LC3EncodeSessionData_fake.call_count, enc_call_count + 1,
			      "Failed to call LC3 encoder %d times",
			      LC3EncodeSessionData_fake.call_count);

		data_l_in += TEST_DEC_MONO_BUF_SIZE;
		data_l_ref += TEST_ENC_MONO_BUF_SIZE;

		enc_call_count += 1;

		memcpy(&audio_data_r_tx, &audio_data, sizeof(struct audio_data));
		memcpy(&audio_data_r_rx, &audio_data, sizeof(struct audio_data));

		audio_data_r_tx.data = (void *)data_r_in;
		audio_data_r_tx.data_size = TEST_DEC_MONO_BUF_SIZE;
		audio_data_r_tx.meta.data_coding = PCM;
		audio_data_r_tx.meta.locations = TEST_AUDIO_MONO_RIGHT_LOCATIONS;

		memset(&data_r[0], 0, TEST_ENC_MONO_BUF_SIZE);
		audio_data_r_rx.data = (void *)&data_r[0];
		audio_data_r_rx.data_size = TEST_ENC_MONO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle[TEST_MODULE_ID_ENCODER_2],
					      &handle[TEST_MODULE_ID_ENCODER_2], &audio_data_r_tx,
					      &audio_data_r_rx, K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for right encoder: %d", ret);
		zassert_equal(audio_data_r_rx.data_size, TEST_ENC_MONO_BUF_SIZE,
			      "Encoder number of bytes for left incorrect: %d",
			      audio_data_r_rx.data_size);
		zassert_mem_equal(&data_r[0], data_r_ref, TEST_ENC_MONO_BUF_SIZE,
				  "Encoder right data does not match reference coded data");
		zassert_equal(LC3EncodeSessionData_fake.call_count, enc_call_count + 1,
			      "Failed to call LC3 encoder %d times",
			      LC3EncodeSessionData_fake.call_count);

		data_r_in += TEST_DEC_MONO_BUF_SIZE;
		data_r_ref += TEST_ENC_MONO_BUF_SIZE;

		enc_call_count += 1;

	} while (((enc_call_count >> 1) < (WAV_MONO_SIZE / TEST_DEC_MONO_BUF_SIZE) &&
		  (enc_call_count >> 1) < (LC3_MONO_SIZE / TEST_ENC_MONO_BUF_SIZE)));

	ret = audio_module_stop(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);
	ret = audio_module_stop(&handle[TEST_MODULE_ID_ENCODER_2]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = audio_module_close(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);
	ret = audio_module_close(&handle[TEST_MODULE_ID_ENCODER_2]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed, called LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

ZTEST(suite_lc3_encoder_functional, test_lc3_encode_multi_deint)
{
	int ret;
	int enc_call_count = 0;
	uint8_t *data_source = (uint8_t *)&wav_mono[0];
	uint8_t *data_reference = (uint8_t *)&lc3_mono[0];
	uint8_t data_in[TEST_DEC_STEREO_BUF_SIZE];
	uint8_t data_out[TEST_ENC_STEREO_BUF_SIZE];
	uint8_t data_ref[TEST_ENC_STEREO_BUF_SIZE];
	struct audio_data audio_data_tx, audio_data_rx;

	/* Fake internal empty data FIFO success */
	ref_in = (uint8_t *)&lc3_mono[0];
	ref_in_size = sizeof(lc3_mono);
	ref_in_read_size = TEST_ENC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3EncodeSessionClose_fake.custom_fake = fake_LC3EncodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3EncodeSessionOpen_fake.custom_fake = fake_LC3EncodeSessionOpen__succeeds;
	LC3EncodeSessionData_fake.custom_fake = fake_LC3EncodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_encoder_multi_init(false, TEST_AUDIO_STEREO_LOCATIONS);

	do {
		memcpy(&data_in[0], data_source, TEST_DEC_MONO_BUF_SIZE);
		memcpy(&data_in[TEST_DEC_MONO_BUF_SIZE], data_source, TEST_DEC_MONO_BUF_SIZE);
		memcpy(&data_ref[0], data_reference, TEST_ENC_MONO_BUF_SIZE);
		memcpy(&data_ref[TEST_ENC_MONO_BUF_SIZE], data_reference, TEST_ENC_MONO_BUF_SIZE);

		memcpy(&audio_data_tx, &audio_data, sizeof(struct audio_data));
		memcpy(&audio_data_rx, &audio_data, sizeof(struct audio_data));

		audio_data_tx.data = &data_in[0];
		audio_data_tx.data_size = TEST_DEC_STEREO_BUF_SIZE;
		audio_data_tx.meta.data_coding = PCM;
		audio_data_tx.meta.locations = TEST_AUDIO_STEREO_LOCATIONS;

		audio_data_rx.data = (void *)&data_out[0];
		audio_data_rx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle[0], &handle[0], &audio_data_tx,
					      &audio_data_rx, K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for encoder: %d", ret);
		zassert_equal(audio_data_rx.data_size, TEST_ENC_STEREO_BUF_SIZE,
			      "Encoder number of bytes incorrect: %d", audio_data_rx.data_size);
		zassert_mem_equal(&data_out[0], &data_ref[0], TEST_ENC_STEREO_BUF_SIZE,
				  "Encoder data does not match reference coded data");
		zassert_equal(LC3EncodeSessionData_fake.call_count, enc_call_count + 2,
			      "Failed to call LC3 encoder %d times",
			      LC3EncodeSessionData_fake.call_count);

		data_source += TEST_DEC_MONO_BUF_SIZE;
		data_reference += TEST_ENC_MONO_BUF_SIZE;

		enc_call_count += 2;

	} while (((enc_call_count >> 1) < (LC3_MONO_SIZE / TEST_ENC_MONO_BUF_SIZE) &&
		  (enc_call_count >> 1) < (WAV_MONO_SIZE / TEST_DEC_MONO_BUF_SIZE)));

	ret = audio_module_stop(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = audio_module_close(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed to call LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

ZTEST(suite_lc3_encoder_functional, test_lc3_encode_multi_int)
{
	int ret;
	int enc_call_count = 0;
	uint8_t *data_source = (uint8_t *)&wav_mono[0];
	uint8_t *data_reference = (uint8_t *)&lc3_mono[0];
	uint8_t data_in[TEST_DEC_STEREO_BUF_SIZE];
	uint8_t data_out[TEST_ENC_STEREO_BUF_SIZE];
	uint8_t data_ref[TEST_ENC_STEREO_BUF_SIZE];
	uint8_t *ref;
	struct audio_data audio_data_tx, audio_data_rx;
	size_t input_remaining = sizeof(lc3_mono);
	size_t ref_remaining = sizeof(wav_mono);

	/* Fake internal empty data FIFO success */
	ref_in = (uint8_t *)&lc3_mono[0];
	ref_in_size = sizeof(lc3_mono);
	ref_in_read_size = TEST_ENC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3EncodeSessionClose_fake.custom_fake = fake_LC3EncodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3EncodeSessionOpen_fake.custom_fake = fake_LC3EncodeSessionOpen__succeeds;
	LC3EncodeSessionData_fake.custom_fake = fake_LC3EncodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_encoder_multi_init(true, TEST_AUDIO_STEREO_LOCATIONS);

	while (input_remaining > TEST_DEC_MONO_BUF_SIZE && ref_remaining > TEST_ENC_MONO_BUF_SIZE) {
		memcpy(&data_ref[0], data_reference, TEST_ENC_MONO_BUF_SIZE);
		memcpy(&data_ref[TEST_ENC_MONO_BUF_SIZE], data_reference, TEST_ENC_MONO_BUF_SIZE);

		/* Interleave the channel samples */
		ref = &data_in[0];
		for (uint32_t i = 0; i < TEST_DEC_MONO_BUF_SIZE;
		     i += CONFIG_LC3_ENCODER_BIT_DEPTH_OCTETS) {
			for (uint8_t j = 0; j < TEST_AUDIO_CHANNELS_STEREO; j++) {
				for (uint8_t k = 0; k < CONFIG_LC3_ENCODER_BIT_DEPTH_OCTETS; k++) {
					*ref++ = data_source[k];
				}
			}

			data_source += CONFIG_LC3_ENCODER_BIT_DEPTH_OCTETS;
		}

		memcpy(&audio_data_tx, &audio_data, sizeof(struct audio_data));
		memcpy(&audio_data_rx, &audio_data, sizeof(struct audio_data));

		audio_data_tx.data = &data_in[0];
		audio_data_tx.data_size = TEST_DEC_STEREO_BUF_SIZE;
		audio_data_tx.meta.data_coding = PCM;
		audio_data_tx.meta.locations = TEST_AUDIO_STEREO_LOCATIONS;

		audio_data_rx.data = (void *)&data_out[0];
		audio_data_rx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle[0], &handle[0], &audio_data_tx,
					      &audio_data_rx, K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for encoder: %d", ret);
		zassert_equal(audio_data_rx.data_size, TEST_ENC_STEREO_BUF_SIZE,
			      "Encoder number of bytes incorrect: %d", audio_data_rx.data_size);
		zassert_mem_equal(audio_data_rx.data, data_ref, TEST_ENC_STEREO_BUF_SIZE,
				  "Encoder data does not match reference coded data");
		zassert_equal(LC3EncodeSessionData_fake.call_count, enc_call_count + 2,
			      "Failed to call LC3 encoder %d times",
			      LC3EncodeSessionData_fake.call_count);

		enc_call_count += 2;

		data_source += TEST_DEC_MONO_BUF_SIZE;

		input_remaining -= TEST_DEC_MONO_BUF_SIZE;
		ref_remaining -= TEST_ENC_MONO_BUF_SIZE;
	}

	ret = audio_module_stop(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = audio_module_close(&handle[TEST_MODULE_ID_ENCODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed to call LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}
