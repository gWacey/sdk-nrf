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
#define TEST_PCM_SAMPLE_RATE_32000	32000
#define TEST_PCM_SAMPLE_RATE_16000	16000
#define TEST_FRAME_SIZE_US		10000
#define TEST_BITS_PER_SAMPLE_16		16
#define TEST_BITS_PER_SAMPLE_24		24
#define TEST_BITS_PER_SAMPLE_32		32
#define TEST_CARRIER_BITS_PER_SAMPLE_16 16
#define TEST_CARRIER_BITS_PER_SAMPLE_32 32
#define TEST_BITS_PER_SAMPLE		TEST_BITS_PER_SAMPLE_16
#define TEST_CARRIER_BITS_PER_SAMPLE TEST_CARRIER_BITS_PER_SAMPLE_16

#define TEST_AUDIO_MONO_LEFT_LOCATIONS	   (1 << AUDIO_CH_L)
#define TEST_AUDIO_MONO_RIGHT_LOCATIONS	   (1 << AUDIO_CH_R)
#define TEST_AUDIO_STEREO_LOCATIONS	   ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))
#define TEST_AUDIO_MULTI_CHANNEL_LOCATIONS ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))

#define TEST_TONE_GEN_ITTERATIONS (4)

#define CONFIG_TEXT   "Tone config"
#define RECONFIG_TEXT "Tone re-config"

struct tone_test_ctx {
	size_t data_size;
	uint8_t sample_bytes;
	uint8_t carrier_bytes;
	uint32_t mix_locations;
	bool interleaved;
	uint32_t locations;
};

static const char *test_instance_name = "Tone instance";
static struct audio_metadata test_metadata = {.data_coding = PCM,
					      .data_len_us = TEST_FRAME_SIZE_US,
					      .sample_rate_hz = TEST_PCM_SAMPLE_RATE_48000,
					      .bits_per_sample = TEST_BITS_PER_SAMPLE,
					      .carried_bits_per_sample =
						      TEST_CARRIER_BITS_PER_SAMPLE_16,
					      .locations = 0x00000001,
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
static struct audio_data audio_data_tone;

static struct audio_module_parameters mod_parameters;

static struct audio_module_tone_gen_configuration configuration = {
	.frequency_hz = 200,
	.amplitude = 1,
	.interleave_output = false,
	.tone_scale = 0.5,
	.input_scale = 0.5
};

static struct audio_module_tone_gen_context context = {0};

static uint8_t test_data_in[TEST_TONE_GEN_MONO_BUF_SIZE];
static uint8_t test_data_out[TEST_TONE_GEN_MONO_BUF_SIZE];
static uint8_t test_data_ref[TEST_TONE_GEN_MONO_BUF_SIZE];

static uint8_t test_tone[FAKE_TONE_LENGTH_BYTES];
static uint32_t test_tone_size;

static void gen_test_tone(void *tone, size_t size, uint8_t samp_bytes, uint8_t carrier_bytes)
{
	uint32_t step;
	int32_t tone_samp;
	uint8_t *tone_8_bit = (uint8_t *)tone;
	uint16_t *tone_16_bit = (uint16_t *)tone;
	uint32_t *tone_32_bit = (uint32_t *)tone;
	uint16_t tone_samples = (size / carrier_bytes) / 4;

	if (samp_bytes == 1) {
		step =  INT8_MAX / (FAKE_TONE_LENGTH_BYTES / 4);
	} else if (samp_bytes == 2) {
		step =  INT16_MAX / (FAKE_TONE_LENGTH_BYTES / 8);
	} else if (samp_bytes == 3) {
		step =  0x007FFFFF / (FAKE_TONE_LENGTH_BYTES / 12);
	} else if (samp_bytes == 4) {
		step =  INT32_MAX / (FAKE_TONE_LENGTH_BYTES / 16);
	} else {
		return;
	}

	for (size_t i = 0; i < tone_samples; i++) {
		tone_samp = i * step;

		if (carrier_bytes == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier_bytes == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i < tone_samples; i++) {
		tone_samp = (step * tone_samples) - (i * step);

		if (carrier_bytes == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier_bytes == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i <tone_samples; i++) {
		tone_samp = -i * step;

		if (carrier_bytes == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier_bytes == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i < tone_samples; i++) {
		tone_samp = (i * step) - (step * tone_samples);

		if (carrier_bytes == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier_bytes == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	test_tone_size = (size - (size % 4));
}

static void generate_ref_multi_chan_buffer(struct audio_data *pcm_cont, struct audio_data *pcm_finite,
		  uint8_t chan_num, bool interleaved, uint32_t *const finite_pos)
{
	uint32_t step;
	uint8_t *output;
	uint8_t carrier_bytes = pcm_cont->meta.carried_bits_per_sample / 8;
	uint32_t tone_pos = *finite_pos;
	uint32_t frame_step;
	uint32_t bytes_per_channel;
	uint32_t out_chan = pcm_finite->meta.locations;
	uint8_t chan;

	bytes_per_channel = pcm_cont->data_size / chan_num;

	if (interleaved) {
		step = carrier_bytes * (chan_num - 1);
		frame_step = step;
	} else {
		step = 0;
		frame_step = bytes_per_channel * (chan_num - 1);
	}

	chan = 0;

	do {
		if (out_chan & 0x01) {
			output = &((uint8_t *)pcm_cont->data)[frame_step * chan];
			tone_pos = *finite_pos;

			for (uint32_t i = 0; i < bytes_per_channel; i += carrier_bytes) {
				for (uint32_t j = 0; j < carrier_bytes; j++) {
					if (tone_pos > (pcm_finite->data_size - 1)) {
						tone_pos = 0;
					}

					*output++ = ((uint8_t *)pcm_finite->data)[tone_pos];
					tone_pos++;
				}

				output += step;
			}

			chan += 1;
			chan_num -= 1;
		} 
		
		out_chan >>= 1;
	} while (chan_num);

	*finite_pos = tone_pos;
}

static void start_up(struct tone_test_ctx *ctx)
{
	int ret;

	gen_test_tone(test_tone, FAKE_TONE_LENGTH_BYTES, ctx->sample_bytes, ctx->carrier_bytes);

	/* Fake tone generation success */
	tone_gen_size_fake.custom_fake = fake_tone_gen_size__succeeds;

	if (handle.state == AUDIO_MODULE_STATE_RUNNING) {
		ret = audio_module_stop(&handle);
		zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	}

	if (handle.state != AUDIO_MODULE_STATE_UNDEFINED) {
		ret = audio_module_close(&handle);
		zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
	}

	configuration.mix_locations = ctx->mix_locations;
	configuration.interleave_output = ctx->interleaved;

	audio_data_tx.meta = test_metadata;
	audio_data_rx.meta = test_metadata;
	audio_data_ref.meta = test_metadata;
	audio_data_tone.meta = test_metadata;

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
}

static void run_test(struct tone_test_ctx *ctx)
{
	int ret;
	int8_t channels;
	uint32_t finite_pos = 0;

	ret = audio_module_number_channels_calculate(ctx->locations, &channels);
	zassert_equal(ret, 0, "Calculate channels function did not return successfully (0): ret %d", ret);

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);

	audio_data_tx.meta.locations = ctx->locations;
	audio_data_ref.meta.locations = ctx->locations;
	audio_data_tone.data = test_tone;
	audio_data_tone.data_size = test_tone_size;
	audio_data_tone.meta.locations = ctx->mix_locations;

	for (int i = 0; i < TEST_TONE_GEN_ITTERATIONS; i++) {
		memset(test_data_in, 0, TEST_TONE_GEN_MONO_BUF_SIZE);
		memset(test_data_out, 0, TEST_TONE_GEN_MONO_BUF_SIZE);
		memset(test_data_ref, 0, TEST_TONE_GEN_MONO_BUF_SIZE);

		audio_data_ref.data = (void *)test_data_ref;
		audio_data_ref.data_size = ctx->data_size;

		generate_ref_multi_chan_buffer(&audio_data_ref, &audio_data_tone,
		  channels, ctx->interleaved, &finite_pos);

		{
			int16_t *ref = (int16_t *)audio_data_ref.data;

			for (int j = 0; j < audio_data_tone.data_size / 2; j++) {
				printk("%4d: %8d\n", j, *ref++);
			}
		}

		audio_data_tx.data = (void *)&test_data_in;
		audio_data_tx.data_size = ctx->data_size;

		audio_data_rx.data = (void *)&test_data_out;
		audio_data_rx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;

		ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx, &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
		zassert_equal(ret, 0, "Data RX function did not return successfully (0): ret %d",
			      ret);
		zassert_mem_equal(audio_data_ref.data, audio_data_rx.data, audio_data_rx.data_size,
				  "Failed to generate tone data");
		zassert_equal(audio_data_ref.data_size, audio_data_rx.data_size,
			      "Failed to generate tone data, sizes differ");
		zassert_mem_equal(&audio_data_ref.meta, &audio_data_rx.meta, sizeof(struct audio_metadata),
				  "Failed to generate tone data, meta data differs");
	}

	finite_pos = 0;
	audio_data_tx.meta.sample_rate_hz = TEST_PCM_SAMPLE_RATE_16000;
	audio_data_ref.meta.sample_rate_hz = TEST_PCM_SAMPLE_RATE_16000;

	for (int i = 0; i < TEST_TONE_GEN_ITTERATIONS; i++) {
		memset(test_data_in, 0, TEST_TONE_GEN_MONO_BUF_SIZE);
		memset(test_data_out, 0, TEST_TONE_GEN_MONO_BUF_SIZE);
		memset(test_data_ref, 0, TEST_TONE_GEN_MONO_BUF_SIZE);

		audio_data_tone.data = test_tone;
		audio_data_tone.data_size = test_tone_size;
		audio_data_tone.meta.locations = ctx->mix_locations;

		audio_data_ref.data = (void *)&test_data_ref;
		audio_data_ref.data_size = ctx->data_size;

		generate_ref_multi_chan_buffer(&audio_data_ref, &audio_data_tone,
		  channels, ctx->interleaved, &finite_pos);

		audio_data_tx.data = (void *)&test_data_in;
		audio_data_tx.data_size = ctx->data_size;

		audio_data_rx.data = (void *)&test_data_out;
		audio_data_rx.data_size = ctx->data_size;

		ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx,  &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
		zassert_equal(ret, 0, "Data RX function did not return successfully (0): ret %d",
			      ret);
#if 0
		{
			int16_t *out = (int16_t *)audio_data_rx.data;
			int16_t *ref = (int16_t *)audio_data_ref.data;

			for (int j = 0; j < audio_data_rx.data_size / 2; j++) {
				printk("%4d: %8d  %8d\n", j, *out++, *ref++);
			}
		}
#endif
		zassert_mem_equal(audio_data_ref.data, audio_data_rx.data, audio_data_rx.data_size,
				  "Failed to generate tone data");
		zassert_equal(audio_data_ref.data_size, audio_data_rx.data_size,
			      "Failed to generate tone data, sizes differ");
		zassert_mem_equal(&audio_data_ref.meta, &audio_data_rx.meta, sizeof(struct audio_metadata),
				  "Failed to generate tone data, meta data differs");
	}

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
}

static void close_down(void)
{
	int ret;

	ret = audio_module_disconnect(&handle, NULL, true);
	zassert_equal(ret, 0, "Disconnect function did not return successfully (0): ret %d", ret);

	ret = audio_module_close(&handle);
	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
}

ZTEST(suite_audio_module_tone_generator, test_module_tone_generator_bad_reconfig)
{
	int ret;
	struct audio_module_tone_gen_configuration test_config;
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000000,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .interleaved = false,
			  .locations = 0x00000001};

	start_up(&ctx);

	test_config.frequency_hz = 10,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 20000,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = 1.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = -0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 1.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = 1.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = -0.5;
	test_config.input_scale = 0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	test_config.frequency_hz = 400,
	test_config.amplitude = 0.5,
	test_config.mix_locations = 0x00000000;
	test_config.tone_scale = 0.5;
	test_config.input_scale = -0.5;
	ret = audio_module_reconfigure(&handle, (struct audio_module_configuration const *const)&test_config);
	zassert_equal(ret, -EINVAL, "Reconfigure function did not return %d: ret %d", -EINVAL, ret);

	close_down();
}

ZTEST(suite_audio_module_tone_generator, test_module_tone_generator_bad_data)
{
	int ret;
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000000,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .interleaved = false,
			  .locations = 0x00000001};

	start_up(&ctx);

	ret = audio_module_start(&handle);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);

	audio_data_tx.data = (void *)test_data_in;
	audio_data_tx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE * 2;
	audio_data_rx.data = (void *)test_data_out;
	audio_data_rx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;
	ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx, &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
	zassert_equal(ret, -ECANCELED, "Data in size > data out size test did not return %d: ret %d", -ECANCELED, ret);

	audio_data_tx.data = (void *)test_data_in;
	audio_data_tx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;
	audio_data_tx.meta.data_coding = LC3;
	audio_data_rx.data = (void *)test_data_out;
	audio_data_rx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;
	ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx, &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
	zassert_equal(ret, -ECANCELED, "Data in size > data out size test did not return %d: ret %d", -ECANCELED, ret);

	audio_data_tx.data = (void *)test_data_in;
	audio_data_tx.data_size = TEST_TONE_GEN_MONO_BUF_SIZE;
	audio_data_tx.meta.data_coding = PCM;
	audio_data_rx.data = (void *)test_data_out;
	audio_data_rx.data_size = 0;
	ret = audio_module_data_tx_rx(&handle, &handle, &audio_data_tx, &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
	zassert_equal(ret, -ECANCELED, "Data in size > data out size test did not return %d: ret %d", -ECANCELED, ret);

	ret = audio_module_stop(&handle);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);

	close_down();
}

ZTEST(suite_audio_module_tone_generator_dint, test_module_tone_generator)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x000000001,
			  .interleaved = false,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000001};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_dint, test_module_tone_generator_multi_channel_deint)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE, 
			  .mix_locations = 0x00000003,
			  .interleaved = false,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_int, test_module_tone_generator_multi_channel_int)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000003,
			  .interleaved = true,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_dint, test_module_tone_generator_reduced)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE - 16,
			  .mix_locations = 0x00000001,
			  .interleaved = false,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000001};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_dint, test_module_tone_generator_multi_channel_reduced_deint)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE - 4,
			  .mix_locations = 0x00000003,
			  .interleaved = true,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_int, test_module_tone_generator_multi_channel_reduced_int)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE - 32,
			  .mix_locations = 0x00000003,
			  .interleaved = true,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_mix, test_module_tone_generator_mix_mono_all)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000001,
			  .interleaved = false,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000001};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_mix, test_module_tone_generator_mix_stereo_all)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000003,
			  .interleaved = false,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_mix, test_module_tone_generator_mix_left)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000001,
			  .interleaved = true,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}

ZTEST(suite_audio_module_tone_generator_mix_z, test_module_tone_generator_mix_right)
{
	struct tone_test_ctx ctx = {.data_size = TEST_TONE_GEN_MONO_BUF_SIZE,
			  .mix_locations = 0x00000002,
			  .interleaved = true,
			  .sample_bytes = sizeof(int16_t),
			  .carrier_bytes = sizeof(int16_t),
			  .locations = 0x00000003};
	
	start_up(&ctx);
	run_test(&ctx);
	close_down();
}
