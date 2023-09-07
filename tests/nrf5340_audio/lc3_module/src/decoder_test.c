/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/debug/thread_analyzer.h>
#include <errno.h>

#include "fakes.h"
#include "ablk_api.h"
#include "amod_api.h"
#include "common.h"
#include "lc3_decoder.h"
#include "sweep21ms_16b48khz_mono_lc3_in.h"
#include "sweep21ms_16b48khz_mono_wav_ref.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(module_testing, 4);

#define CONFIG_DECODER_MSG_QUEUE_SIZE	   (4)
#define CONFIG_DATA_OBJECTS_NUM		   (4)
#define CONFIG_LC3_DECODER_STACK_SIZE	   (10000)
#define CONFIG_LC3_DECODER_THREAD_PRIORITY (4)
#define CONFIG_MODULES_NUM		   (3)
#define CONFIG_DECODER_MODULES_NUM	   (2)
#define CONFIG_ENCODER_MODULES_NUM	   (1)
#define CONFIG_DECODER_BUFFER_NUM	   (8)
#define CONFIG_DECODER_BUFFER_ALIGN	   (4)

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
		  CONFIG_DECODER_BUFFER_ALIGN);

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

static void lc3_initialize(int duration_us)
{
	int ret;
	uint8_t enc_sample_rates = 0;
	uint8_t dec_sample_rates = 0;
	uint8_t unique_session = 0;
	LC3FrameSize_t framesize;

	/* Set unique session to 0 for using the default sharing memory setting.
	 *
	 * This could lead to higher heap consumption, but is able to manipulate
	 * different sample rate setting between encoder/decoder.
	 */

	/* Check supported sample rates for encoder */
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_8KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_8_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_16KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_16_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_24KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_24_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_32KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_32_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_441KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_441_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_ENC_SAMPLE_RATE_48KHZ_SUPPORT)) {
		enc_sample_rates |= LC3_SAMPLE_RATE_48_KHZ;
	}

	/* Check supported sample rates for decoder */
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_8KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_8_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_16KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_16_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_24KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_24_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_32KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_32_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_441KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_441_KHZ;
	}
	if (IS_ENABLED(CONFIG_LC3_DEC_SAMPLE_RATE_48KHZ_SUPPORT)) {
		dec_sample_rates |= LC3_SAMPLE_RATE_48_KHZ;
	}

	switch (duration_us) {
	case 7500:
		framesize = LC3FrameSize7_5Ms;
		break;
	case 10000:
		framesize = LC3FrameSize10Ms;
		break;
	default:
		LOG_ERR("Unsupported framesize: %d", duration_us);
		return;
	}

	ret = LC3Initialize(enc_sample_rates, dec_sample_rates, framesize, unique_session, NULL,
			    NULL);
	zassert_equal(ret, 0, "LC3 initialize did not return zero");
}

static void lc3_deinitialize(void)
{
	int ret;

	ret = LC3Deinitialize();
	zassert_equal(ret, 0, "LC3 codec failed initialization");
}

static void test_lc3_decoder_mono_init(void)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bits_per_sample = TEST_SAMPLE_BIT_DEPTH;
	test_decoder_config.carrier_size = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.interleaved = ABLK_DEINTERLEAVED;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L);

	/* Need a data_fifo_deinit() */
	lc3_dec1_fifo_rx.initialized = false;
	lc3_dec1_fifo_tx.initialized = false;

	data_fifo_init(&lc3_dec1_fifo_rx);
	data_fifo_init(&lc3_dec1_fifo_tx);

	test_decoder_param.description = lc3_decoder_description;
	test_decoder_param.thread.stack = lc3_dec1_thread_stack,
	test_decoder_param.thread.stack_size = CONFIG_LC3_DECODER_STACK_SIZE;
	test_decoder_param.thread.priority = CONFIG_LC3_DECODER_THREAD_PRIORITY;
	test_decoder_param.thread.data_slab = &audio_data_slab;
	test_decoder_param.thread.data_size = TEST_DEC_MONO_BUF_SIZE;
	test_decoder_param.thread.msg_rx = &lc3_dec1_fifo_rx;
	test_decoder_param.thread.msg_tx = &lc3_dec1_fifo_tx;

	if (IS_ENABLED(CONFIG_THREAD_ANALYZER)) {
		thread_analyzer_print();
	}

	ret = amod_open(&test_decoder_param, (struct amod_configuration *)&test_decoder_config,
			"Decoder Left",
			(struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder module open did not return zero");
	zassert_equal(LC3DecodeSessionClose_fake.call_count, 0,
		      "Called LC3 close decoder session %d times",
		      LC3DecodeSessionClose_fake.call_count);
	zassert_equal(LC3BitstreamBuffersize_fake.call_count, 1,
		      "Failed to call LC3 get buffer size %d times",
		      LC3BitstreamBuffersize_fake.call_count);
	zassert_equal(LC3DecodeSessionOpen_fake.call_count, 1,
		      "Failed to call LC3 open decoder session %d times",
		      LC3DecodeSessionOpen_fake.call_count);

	if (IS_ENABLED(CONFIG_THREAD_ANALYZER)) {
		thread_analyzer_print();
	}

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_1], &handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder connect did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module start did not return zero");
}

static void test_lc3_decoder_dual_mono_init(void)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bits_per_sample = TEST_SAMPLE_BIT_DEPTH;
	test_decoder_config.carrier_size = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.interleaved = ABLK_DEINTERLEAVED;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L);

	/* Need a data_fifo_deinit() */
	lc3_dec1_fifo_rx.initialized = false;
	lc3_dec1_fifo_tx.initialized = false;

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
			"Decoder Left",
			(struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module open did not return zero");
	zassert_equal(LC3DecodeSessionClose_fake.call_count, 0,
		      "Called LC3 close decoder session %d times",
		      LC3DecodeSessionClose_fake.call_count);
	zassert_equal(LC3BitstreamBuffersize_fake.call_count, 1,
		      "Failed to call LC3 get buffer size %d times",
		      LC3BitstreamBuffersize_fake.call_count);
	zassert_equal(LC3DecodeSessionOpen_fake.call_count, 1,
		      "Failed to call LC3 open decoder session %d times",
		      LC3DecodeSessionOpen_fake.call_count);

	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_R);

	/* Need a data_fifo_deinit() */
	lc3_dec2_fifo_rx.initialized = false;
	lc3_dec2_fifo_tx.initialized = false;

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
			"Decoder Right",
			(struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_2],
			&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Decoder right module open did not return zero");
	zassert_equal(LC3DecodeSessionClose_fake.call_count, 0,
		      "Called LC3 close decoder session %d times",
		      LC3DecodeSessionClose_fake.call_count);
	zassert_equal(LC3BitstreamBuffersize_fake.call_count, 2,
		      "Failed to call LC3 get buffer size %d times",
		      LC3BitstreamBuffersize_fake.call_count);
	zassert_equal(LC3DecodeSessionOpen_fake.call_count, 2,
		      "Failed to call LC3 open decoder session %d times",
		      LC3DecodeSessionOpen_fake.call_count);

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_1], &handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder connect did not return zero");

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_2], &handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Decoder connect did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Left decoder module start did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Right decoder module start did not return zero");
}

#ifdef NOT_YET
static void test_lc3_decoder_stereo_init(enum ablk_interleaved interleaved)
{
	int ret;
	struct amod_parameters test_decoder_param;
	struct lc3_decoder_configuration test_decoder_config;

	test_decoder_config.sample_rate = TEST_PCM_SAMPLE_RATE;
	test_decoder_config.bits_per_sample = TEST_SAMPLE_BIT_DEPTH;
	test_decoder_config.carrier_size = TEST_PCM_BIT_DEPTH;
	test_decoder_config.max_bitrate = TEST_LC3_BITRATE;
	test_decoder_config.duration_us = TEST_LC3_FRAME_SIZE_US;
	test_decoder_config.interleaved = interleaved;
	test_decoder_config.channel_map = BIT(TEST_AUDIO_CH_L) | BIT(TEST_AUDIO_CH_R);

	/* Need a data_fifo_deinit() */
	lc3_dec1_fifo_rx.initialized = false;
	lc3_dec1_fifo_tx.initialized = false;

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
			"Decoder Left",
			(struct amod_context *)&decoder_ctx[TEST_MODULE_ID_DECODER_1],
			&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module open did not return zero");
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);
	zassert_equal(LC3DecodeSessionClose_fake.call_count, 0,
		      "Called LC3 close decoder session %d times",
		      LC3DecodeSessionClose_fake.call_count);
	zassert_equal(LC3BitstreamBuffersize_fake.call_count, 1,
		      "Failed to call LC3 get buffer size %d times",
		      LC3BitstreamBuffersize_fake.call_count);
	zassert_equal(LC3DecodeSessionOpen_fake.call_count, 2,
		      "Failed to call LC3 open decoder session %d times",
		      LC3DecodeSessionOpen_fake.call_count);

	ret = amod_connect(&handle[TEST_MODULE_ID_DECODER_1], &handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder connect did not return zero");

	ret = amod_start(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Decoder left module start did not return zero");
}
#endif /* NOT_YET */

ZTEST(suite_lc3_decoder_functional, test_lc3_decode_mono)
{
	int ret;
	int dec_call_count = 1;
	uint8_t *data_in = &lc3_mono_in[0];
	uint8_t *data_ref = (uint8_t *)&wav_mono_ref[0];
	uint8_t pcm_out[TEST_DEC_MONO_BUF_SIZE];
	struct ablk_block block_tx, block_rx;

	/* Fake internal empty data FIFO success */
	wav_out = (uint8_t *)&wav_mono_ref[0];
	test_dec_out_size = TEST_DEC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3DecodeSessionClose_fake.custom_fake = fake_LC3DecodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3DecodeSessionOpen_fake.custom_fake = fake_LC3DecodeSessionOpen__succeeds;
	LC3DecodeSessionData_fake.custom_fake = fake_LC3DecodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed, called LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_decoder_mono_init();

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = (void *)data_in;
		block_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_rx.data_type = ABLK_TYPE_PCM;
		block_rx.data = (void *)&pcm_out[0];
		block_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for the decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&pcm_out[0], data_ref, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");
		zassert_equal(LC3DecodeSessionData_fake.call_count, dec_call_count,
			      "Failed to call LC3 decode %d times",
			      LC3DecodeSessionData_fake.call_count);

		data_in += TEST_ENC_MONO_BUF_SIZE;
		data_ref += TEST_DEC_MONO_BUF_SIZE;

		dec_call_count += 1;

	} while ((dec_call_count <= (LC3_MONO_IN_SIZE / TEST_ENC_MONO_BUF_SIZE) &&
		  dec_call_count <= (WAV_MONO_REF_SIZE / TEST_DEC_MONO_BUF_SAMPLES)));

	ret = amod_stop(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = amod_close(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed, called LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

ZTEST(suite_lc3_decoder_functional, test_lc3_decode_dual_mono)
{
	int ret;
	int dec_call_count = 1;
	uint8_t *data_l_in = (uint8_t *)&lc3_mono_in[0];
	uint8_t *data_r_in = (uint8_t *)&lc3_mono_in[0];
	uint8_t data_l[TEST_DEC_MONO_BUF_SIZE], data_r[TEST_DEC_MONO_BUF_SIZE];
	uint8_t *data_l_ref = (uint8_t *)&wav_mono_ref[0];
	uint8_t *data_r_ref = (uint8_t *)&wav_mono_ref[0];
	struct ablk_block block_l_tx, block_l_rx;
	struct ablk_block block_r_tx, block_r_rx;

	/* Fake internal empty data FIFO success */
	wav_out = (uint8_t *)&wav_mono_ref[0];
	test_dec_out_size = TEST_DEC_MONO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3DecodeSessionClose_fake.custom_fake = fake_LC3DecodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3DecodeSessionOpen_fake.custom_fake = fake_LC3DecodeSessionOpen__succeeds;
	LC3DecodeSessionData_fake.custom_fake = fake_LC3DecodeSessionData__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed, called LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_decoder_dual_mono_init();

	do {
		memcpy(&block_l_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_l_rx, &test_block, sizeof(struct ablk_block));

		block_l_tx.data_type = ABLK_TYPE_LC3;
		block_l_tx.data = (void *)data_l_in;
		block_l_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_l_rx.data = (void *)&data_l[0];
		block_l_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_l_tx, &block_l_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_l_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes for right incorrect: %d",
			      block_l_rx.data_size);
		zassert_mem_equal(&data_l[0], data_l_ref, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded left PCM data does not match reference PCM data");
		zassert_equal(LC3DecodeSessionData_fake.call_count, dec_call_count,
			      "Failed to call LC3 decode %d times",
			      LC3DecodeSessionData_fake.call_count);

		data_l_in += TEST_ENC_MONO_BUF_SIZE;
		data_l_ref += TEST_DEC_MONO_BUF_SIZE;

		dec_call_count += 1;

		memcpy(&block_r_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_r_rx, &test_block, sizeof(struct ablk_block));

		block_r_tx.data_type = ABLK_TYPE_LC3;
		block_r_tx.data = (void *)data_r_in;
		block_r_tx.data_size = TEST_ENC_MONO_BUF_SIZE;

		block_r_rx.data = (void *)&data_r[0];
		block_r_rx.data_size = TEST_DEC_MONO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_2],
				      &handle[TEST_MODULE_ID_DECODER_2], &block_r_tx, &block_r_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for right decoder: %d", ret);
		zassert_equal(block_r_rx.data_size, TEST_DEC_MONO_BUF_SIZE,
			      "Decoded number of bytes for left incorrect: %d",
			      block_r_rx.data_size);
		zassert_mem_equal(&data_r[0], data_r_ref, TEST_DEC_MONO_BUF_SIZE,
				  "Decoded right PCM data does not match reference PCM data");
		zassert_mem_equal(&data_r[0], data_r_ref, TEST_DEC_MONO_BUF_SIZE,
				  "Left PCM data does not match right PCM data");
		zassert_equal(LC3DecodeSessionData_fake.call_count, dec_call_count,
			      "Failed to call LC3 decode %d times",
			      LC3DecodeSessionData_fake.call_count);

		data_r_in += TEST_ENC_MONO_BUF_SIZE;
		data_r_ref += TEST_DEC_MONO_BUF_SIZE;

		dec_call_count += 1;

	} while (((dec_call_count >> 1) < (LC3_MONO_IN_SIZE / TEST_ENC_MONO_BUF_SIZE) &&
		  (dec_call_count >> 1) < (WAV_MONO_REF_SIZE / TEST_DEC_MONO_BUF_SAMPLES)));

	ret = amod_stop(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);
	ret = amod_stop(&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = amod_close(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);
	ret = amod_close(&handle[TEST_MODULE_ID_DECODER_2]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed, called LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

#ifdef NOT_YET
ZTEST(suite_lc3_decoder_functional, test_lc3_decode_stereo_deint)
{
	int ret;
	int dec_call_count = 2;
	uint8_t *data_in = (uint8_t *)&lc3_stereo_in[0];
	uint8_t data_out[TEST_DEC_MONO_BUF_SIZE];
	uint8_t *data_ref = (uint8_t *)&wav_stereo_ref[0];
	struct ablk_block block_tx, block_rx;

	/* Fake internal empty data FIFO success */
	wav_out = (uint8_t *)&wav_stereo_ref[0];
	test_dec_out_size = TEST_DEC_STEREO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3DecodeSessionClose_fake.custom_fake = fake_LC3DecodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3DecodeSessionOpen_fake.custom_fake = fake_LC3DecodeSessionOpen__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_decoder_stereo_init(ABLK_DEINTERLEAVED);

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = data_in;
		block_tx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		block_rx.data = (void *)&data_out[0];
		block_rx.data_size = TEST_DEC_STEREO_BUF_SIZE;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_STEREO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&data[0], ref_data_pcm, TEST_DEC_STEREO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");
		zassert_equal(LC3DecodeSessionData_fake.call_count, dec_call_count,
			      "Failed to call LC3 decode %d times",
			      LC3DecodeSessionData_fake.call_count);

		input_data_lc3 += TEST_ENC_STEREO_BUF_SIZE;
		ref_data_pcm += TEST_DEC_STEREO_BUF_SIZE;

		dec_call_count += 2;

	} while (((dec_call_count / 2) <= (LC3_STEREO_IN_SIZE / TEST_ENC_STEREO_BUF_SIZE) &&
		  (dec_call_count / 2) <= (WAV_STEREO_REF_SIZE / TEST_DEC_STEREO_BUF_SAMPLES)));

	ret = amod_stop(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = amod_close(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed to call LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}

ZTEST(suite_lc3_decoder_functional, test_lc3_decode_stereo_int)
{
	int ret;
	int dec_call_count = 2;
	uint8_t *data_in = (uint8_t *)&lc3_stereo_in[0];
	uint8_t data_out[TEST_DEC_MONO_BUF_SIZE];
	uint8_t *data_ref = (uint8_t *)&wav_stereo_ref[0];
	struct ablk_block block_tx, block_rx;

	/* Fake internal empty data FIFO success */
	wav_out = (uint8_t *)&wav_stereo_ref[0];
	test_dec_out_size = TEST_DEC_STEREO_BUF_SIZE;
	LC3Initialize_fake.custom_fake = fake_LC3Initialize__succeeds;
	LC3DecodeSessionClose_fake.custom_fake = fake_LC3DecodeSessionClose__succeeds;
	LC3BitstreamBuffersize_fake.custom_fake = fake_LC3BitstreamBuffersize__succeeds;
	LC3DecodeSessionOpen_fake.custom_fake = fake_LC3DecodeSessionOpen__succeeds;
	LC3Deinitialize_fake.custom_fake = fake_LC3Deinitialize__succeeds;

	lc3_initialize(TEST_LC3_FRAME_SIZE_US);
	zassert_equal(LC3Initialize_fake.call_count, 1, "Failed to call LC3 initialize %d times",
		      LC3Initialize_fake.call_count);

	test_lc3_decoder_stereo_init(ABLK_INTERLEAVED);

	do {
		memcpy(&block_tx, &test_block, sizeof(struct ablk_block));
		memcpy(&block_rx, &test_block, sizeof(struct ablk_block));

		block_tx.data_type = ABLK_TYPE_LC3;
		block_tx.data = data_in;
		block_tx.data_size = TEST_ENC_STEREO_BUF_SIZE;

		block_rx.data = data_out;
		block_rx.data_size = TEST_DEC_STEREO_BUF_SIZE;
		block_rx.interleaved = ABLK_INTERLEAVED;

		ret = amod_data_tx_rx(&handle[TEST_MODULE_ID_DECODER_1],
				      &handle[TEST_MODULE_ID_DECODER_1], &block_tx, &block_rx,
				      K_FOREVER);
		zassert_equal(ret, 0, "Data TX-RX did not return zero for left decoder: %d", ret);
		zassert_equal(block_rx.data_size, TEST_DEC_STEREO_BUF_SIZE,
			      "Decoded number of bytes incorrect: %d");
		zassert_mem_equal(&data_out[0], data_ref, TEST_DEC_STEREO_BUF_SIZE,
				  "Decoded PCM data does not match reference PCM data");
		zassert_equal(LC3DecodeSessionData_fake.call_count, dec_call_count,
			      "Failed to call LC3 decode %d times",
			      LC3DecodeSessionData_fake.call_count);

		data_in += TEST_ENC_STEREO_BUF_SIZE;
		data_ref += TEST_DEC_STEREO_BUF_SIZE;

		dec_call_count += 2;

	} while (((dec_call_count / 2) <= (LC3_STEREO_IN_SIZE / TEST_ENC_STEREO_BUF_SIZE) &&
		  (dec_call_count / 2) <= (WAV_STEREO_REF_SIZE / TEST_DEC_STEREO_BUF_SAMPLES)));

	ret = amod_stop(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to stop module did not return zero: ret = %d", ret);

	ret = amod_close(&handle[TEST_MODULE_ID_DECODER_1]);
	zassert_equal(ret, 0, "Failed to close module did not return zero: ret = %d", ret);

	lc3_deinitialize();
	zassert_equal(LC3Deinitialize_fake.call_count, 1,
		      "Failed to call LC3 deinitialize %d times", LC3Deinitialize_fake.call_count);
}
#endif /* NOT_YET */
