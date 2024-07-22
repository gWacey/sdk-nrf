/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <errno.h>

#include "fakes.h"
#include "tone.h"
#include "audio_defines.h"
#include "audio_module.h"
#include "tone_generator.h"

#define TEST_TONE_GEN_MSG_QUEUE_SIZE	   (4)
#define TEST_TONE_GEN_DATA_OBJECTS_NUM	   (2)
#define TEST_TONE_GEN_STACK_SIZE	   (4192)
#define TEST_TONE_GEN_THREAD_PRIORITY	   (4)
#define TEST_TONE_GEN_MODULES_NUM	   (TEST_AUDIO_CHANNELS_MAX)
#define TEST_TONE_GEN_MSG_SIZE		   (sizeof(struct audio_module_message))
#define TEST_TONE_GEN_AUDIO_DATA_ITEMS_NUM (4)

/* Define a timeout to prevent system locking */
#define TEST_TX_RX_TIMEOUT_US (K_FOREVER)

#define TEST_AUDIO_CHANNELS_MONO      1
#define TEST_AUDIO_CHANNELS_DUAL_MONO 2
#define TEST_AUDIO_CHANNELS_STEREO    2
#define TEST_AUDIO_CHANNELS_MAX	      2

#define TEST_SAMPLE_BIT_DEPTH		 16
#define TEST_PCM_BIT_DEPTH		 16
#define TEST_TONE_GEN_MONO_BUF_SIZE	 960
#define TEST_TONE_GEN_MONO_BUF_SAMPLES	 (TEST_TONE_GEN_MONO_BUF_SIZE / (TEST_PCM_BIT_DEPTH / 8))
#define TEST_TONE_GEN_STEREO_BUF_SIZE	 (TEST_TONE_GEN_MONO_BUF_SIZE * TEST_AUDIO_CHANNELS_STEREO)
#define TEST_TONE_GEN_STEREO_BUF_SAMPLES (TEST_TONE_GEN_STEREO_BUF_SIZE / (TEST_PCM_BIT_DEPTH / 8))
#define TEST_TONE_GEN_MULTI_BUF_SIZE	 (TEST_TONE_GEN_MONO_BUF_SIZE * TEST_AUDIO_CHANNELS_MAX)
#define TEST_TONE_GEN_MULTI_BUF_SAMPLES	 (TEST_TONE_GEN_MULTI_BUF_SIZE / (TEST_PCM_BIT_DEPTH / 8))

#define TEST_PCM_SAMPLE_RATE_48000	48000
#define TEST_PCM_SAMPLE_RATE_16000	16000
#define TEST_FRAME_SIZE_US		10000
#define TEST_BITS_PER_SAMPLE		16
#define TEST_CARRIER_BITS_PER_SAMPLE_16 16
#define TEST_CARRIER_BITS_PER_SAMPLE_32 32

#define TEST_AUDIO_MONO_LEFT_LOCATIONS	   (1 << AUDIO_CH_L)
#define TEST_AUDIO_MONO_RIGHT_LOCATIONS	   (1 << AUDIO_CH_R)
#define TEST_AUDIO_STEREO_LOCATIONS	   ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))
#define TEST_AUDIO_MULTI_CHANNEL_LOCATIONS ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))

#define CONFIG_TEXT   "Tone config"
#define RECONFIG_TEXT "Tone re-config"

static const char *test_instance_name = "Tone instance";
static struct audio_metadata test_metadata = {.data_coding = PCM,
					      .data_len_us = TEST_FRAME_SIZE_US,
					      .sample_rate_hz = TEST_PCM_SAMPLE_RATE_48000,
					      .bits_per_sample = TEST_BITS_PER_SAMPLE,
					      .carried_bits_per_sample =
						      TEST_CARRIER_BITS_PER_SAMPLE_16,
					      .locations = 0x00000003,
					      .reference_ts_us = 0,
					      .data_rx_ts_us = 0,
					      .bad_data = false};

K_THREAD_STACK_DEFINE(tone_gen_thread_stack, CONFIG_TONE_GENERATOR_STACK_SIZE);
K_MEM_SLAB_DEFINE(mod_data_slab, TEST_TONE_GEN_MONO_BUF_SIZE, TEST_TONE_GEN_DATA_OBJECTS_NUM, 4);
DATA_FIFO_DEFINE(msg_fifo_tx, TEST_TONE_GEN_MSG_QUEUE_SIZE, TEST_TONE_GEN_MSG_SIZE);
DATA_FIFO_DEFINE(msg_fifo_rx, TEST_TONE_GEN_MSG_QUEUE_SIZE, TEST_TONE_GEN_MSG_SIZE);

static struct audio_module_handle handle;

static struct audio_data audio_data_tx;
static struct audio_data audio_data_rx;
static struct audio_data audio_data_ref;

static struct audio_module_parameters mod_parameters;

static struct audio_module_tone_gen_configuration configuration = {
	.frequency_hz = 200,
	.amplitude = 1,
	.interleaved = false};

static struct audio_module_tone_gen_context context = {0};

static uint8_t test_data_in[TEST_TONE_GEN_MONO_BUF_SIZE * TEST_TONE_GEN_DATA_OBJECTS_NUM];
static uint8_t test_data_out[TEST_TONE_GEN_MONO_BUF_SIZE * TEST_TONE_GEN_DATA_OBJECTS_NUM];

ZTEST(suite_audio_module_tone_generator, test_module_tone_generator)
{
	int ret;

	/* Fake tone generation success */
	tone_gen_fake.custom_fake = fake_tone_gen__succeeds;

	audio_data_ref.meta = test_metadata;
	audio_data_tx.meta = test_metadata;
	audio_data_rx.meta = test_metadata;

	data_fifo_init(&msg_fifo_tx);
	data_fifo_init(&msg_fifo_rx);

	AUDIO_MODULE_PARAMETERS(mod_parameters, audio_module_tone_gen_description,
				tone_gen_thread_stack, CONFIG_TONE_GENERATOR_STACK_SIZE,
				CONFIG_TONE_GENERATOR_THREAD_PRIO, &msg_fifo_rx, &msg_fifo_tx,
				&mod_data_slab, TEST_TONE_GEN_MONO_BUF_SIZE);

	ret = audio_module_open(
		&mod_parameters, (struct audio_module_configuration const *const)&configuration,
		test_instance_name, (struct audio_module_context *)&context, &handle);
	zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);

	ret = audio_module_connect(&handle, NULL, true);
	zassert_equal(ret, 0, "Connect function did not return successfully (0): ret %d", ret);

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);

	for (int i = 0; i < TEST_TONE_GEN_DATA_OBJECTS_NUM; i++) {
		printk("Process audio buffer %d\n", i);

		audio_data_ref.data = (void *)&test_data_out[TEST_TONE_GEN_MONO_BUF_SIZE];
		audio_data_ref.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;

		audio_data_tx.data = (void *)&test_data_in[TEST_TONE_GEN_MONO_BUF_SIZE];
		audio_data_tx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;

		audio_data_rx.data = (void *)&test_data_out[TEST_TONE_GEN_MONO_BUF_SIZE];
		audio_data_rx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx,  &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
		zassert_equal(ret, 0, "Data RX function did not return successfully (0): ret %d",
			      ret);
		zassert_mem_equal(audio_data_ref.data, audio_data_rx.data, TEST_TONE_GEN_MONO_BUF_SIZE,
				  "Failed to generate tone data");
		zassert_equal(audio_data_ref.data_size, audio_data_rx.data_size,
			      "Failed to generate tone data, sizes differ");
		zassert_mem_equal(&audio_data_ref.meta, &audio_data_rx.meta, sizeof(struct audio_metadata),
				  "Failed to generate tone data, meta data differs");
	}

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);

	ret = audio_module_disconnect(&handle, NULL, true);
	zassert_equal(ret, 0, "Disconnect function did not return successfully (0): ret %d", ret);

	ret = audio_module_close(&handle);
	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
}
