/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <errno.h>

#include "audio_module.h"
#include "test_common.h"

/* Define a timeout to prevent system locking */
#define TEST_TX_RX_TIMEOUT_US (K_FOREVER)

#define CONFIG_TEXT   "Tone config"
#define RECONFIG_TEXT "Tone re-config"

static const char *test_instance_name = "Test instance";
static struct audio_metadata test_metadata = {.data_coding = PCM,
					      .data_len_us = 10000,
					      .sample_rate_hz = 48000,
					      .bits_per_sample = 16,
					      .carried_bits_per_sample = 16,
					      .locations = 0x00000003,
					      .reference_ts_us = 0,
					      .data_rx_ts_us = 0,
					      .bad_data = false};

K_THREAD_STACK_ARRAY_DEFINE(mod_temp_stack, TEST_MODULES_NUM, TONE_GENERATOR_STACK_SIZE);
K_MEM_SLAB_DEFINE(mod_data_slab, TEST_MOD_DATA_SIZE, FAKE_FIFO_MSG_QUEUE_SIZE, 4);

static struct audio_module_handle handle[TEST_MODULES_NUM];

static struct data_fifo msg_fifo_tx[TEST_MODULES_NUM];

static struct audio_data audio_data_rx;

static struct audio_module_parameters mod_parameters;

static struct audio_module_tone_gen_configuration configuration = {.sample_rate_hz = 48000,
								   .bits_per_sample = 16,
								   .carried_bits_per_sample = 16,
								   .amplitude = 1,
								   .interleaved = false};
static struct audio_module_tone_gen_configuration reconfiguration = {.sample_rate_hz = 16000,
								     .bits_per_sample = 16,
								     .carried_bits_per_sample = 32,
								     .amplitude = 1,
								     .interleaved = false};
static struct audio_module_tone_gen_configuration return_configuration;

static struct audio_module_tone_gen_context context = {0};

static uint8_t test_data_out[TEST_MOD_DATA_SIZE * TEST_AUDIO_DATA_ITEMS_NUM];

ZTEST(suite_audio_module_tone_generator, test_module_tone_generator)
{
	int ret;
	int i;
	char *base_name, instance_name[CONFIG_AUDIO_MODULE_NAME_SIZE];

	/* Fake internal empty data FIFO success */
	data_fifo_init_fake.custom_fake = fake_data_fifo_init__succeeds;
	data_fifo_pointer_first_vacant_get_fake.custom_fake =
		fake_data_fifo_pointer_first_vacant_get__succeeds;
	data_fifo_block_lock_fake.custom_fake = fake_data_fifo_block_lock__succeeds;
	data_fifo_pointer_last_filled_get_fake.custom_fake =
		fake_data_fifo_pointer_last_filled_get__succeeds;
	data_fifo_block_free_fake.custom_fake = fake_data_fifo_block_free__succeeds;

	data_fifo_deinit(&msg_fifo_rx[0]);
	data_fifo_deinit(&msg_fifo_tx[0]);

	AUDIO_MODULE_PARAMETERS(mod_parameters, audio_module_tone_gen_description, mod_temp_stack,
				TONE_GENERATOR_STACK_SIZE, TONE_GENERATOR_THREAD_PRIO, NULL,
				&msg_fifo_tx[0], &mod_data_slab, TEST_MOD_DATA_SIZE);

	ret = audio_module_open(
		&mod_parameters, (struct audio_module_configuration const *const)&configuration,
		test_instance_name, (struct audio_module_context *)&context, &handle[0]);
	zassert_equal(ret, 0, "Open function did not return successfully (0): ret %d", ret);

	{
		struct audio_module_tone_gen_context *ctx =
			(struct audio_module_tone_gen_context *)handle[0].context;
		struct audio_module_tone_gen_configuration *test_config = &ctx->config;

		zassert_equal(test_config->sample_rate_hz, configuration.sample_rate_hz,
			      "Failed to configure module in the open function");
		zassert_equal(test_config->bits_per_sample, configuration.bits_per_sample,
			      "Failed to configure module in the open function");
		zassert_equal(test_config->carried_bits_per_sample,
			      configuration.carried_bits_per_sample,
			      "Failed to configure module in the open function");
		zassert_equal(test_config->amplitude, configuration.amplitude,
			      "Failed to configure module in the open function");
		zassert_equal(test_config->interleaved, configuration.interleaved,
			      "Failed to configure module in the open function");
	}

	zassert_equal_ptr(handle[0].description->name, audio_module_tone_gen_description->name,
			  "Failed open for names, base name should be %s, but is %s",
			  audio_module_tone_gen_description->name, handle[0].description->name);
	zassert_mem_equal(&handle[0].name[0], test_instance_name, strlen(test_instance_name),
			  "Failed open for names, instance name should be %s, but is %s",
			  test_instance_name, handle[0].name);
	zassert_equal(handle[0].state, AUDIO_MODULE_STATE_CONFIGURED,
		      "Not in configured state after call to open the audio module");

	ret = audio_module_names_get(&handle[0], &base_name, &instance_name[0]);
	zassert_equal(ret, 0, "Get names function did not return successfully (0): ret %d", ret);
	zassert_mem_equal(base_name, audio_module_tone_gen_description->name,
			  strlen(audio_module_tone_gen_description->name),
			  "Failed get names, base name should be %s, but is %s",
			  audio_module_tone_gen_description->name, base_name);
	zassert_mem_equal(&instance_name[0], test_instance_name, strlen(test_instance_name),
			  "Failed get names, instance name should be %s, but is %s", handle[0].name,
			  &instance_name[0]);

	ret = audio_module_configuration_get(
		&handle[0], (struct audio_module_configuration *)&return_configuration);
	zassert_equal(ret, 0, "Configuration get function did not return successfully (0): ret %d",
		      ret);
	zassert_equal(test_config->sample_rate_hz, configuration.sample_rate_hz,
		      "Failed to configure module in the open function");
	zassert_equal(test_config->bits_per_sample, configuration.bits_per_sample,
		      "Failed to configure module in the open function");
	zassert_equal(test_config->carried_bits_per_sample, configuration.carried_bits_per_sample,
		      "Failed to configure module in the open function");
	zassert_equal(test_config->amplitude, configuration.amplitude,
		      "Failed to configure module in the open function");
	zassert_equal(test_config->interleaved, configuration.interleaved,
		      "Failed to configure module in the open function");

	ret = audio_module_connect(&handle[0], NULL, true);
	zassert_equal(ret, 0, "Connect function did not return successfully (0): ret %d", ret);
	zassert_equal(handle[0].use_tx_queue, 1, "Connect queue size is not %d, but is %d", 1,
		      handle[0].use_tx_queue);

	ret = audio_module_start(&handle[0]);
	zassert_equal(ret, 0, "Start function did not return successfully (0): ret %d", ret);
	zassert_equal(handle[0].state, AUDIO_MODULE_STATE_RUNNING,
		      "Not in running state after call to start the audio module");

	for (i = 0; i < TEST_AUDIO_DATA_ITEMS_NUM; i++) {
		printk("Process audio buffer %d\n", i);

		audio_data_rx.data = (void *)&test_data_out[i * TEST_MOD_DATA_SIZE];
		audio_data_rx.data_size = TEST_MOD_DATA_SIZE;

		ret = audio_module_data_rx(&handle[0], &audio_data_rx, TEST_TX_RX_TIMEOUT_US);
		zassert_equal(ret, 0, "Data RX function did not return successfully (0): ret %d",
			      ret);
		zassert_mem_equal(ref_data, audio_data_rx.data, TEST_MOD_DATA_SIZE,
				  "Failed to generate tone data");
		zassert_equal(audio_data_tx.data_size, audio_data_rx.data_size,
			      "Failed to generate tone data, sizes differ");
		zassert_mem_equal(ref_meta, &audio_data_rx.meta, sizeof(struct audio_metadata),
				  "Failed to generate tone data, meta data differs");

		if (i == TEST_AUDIO_DATA_ITEMS_NUM / 2) {
			ret = audio_module_stop(&handle[0]);
			zassert_equal(ret, 0,
				      "Stop function did not return successfully (0): ret %d", ret);

			ret = audio_module_reconfigure(
				&handle[0],
				(const struct audio_module_configuration *const)&reconfiguration);
			zassert_equal(
				ret, 0,
				"Reconfigure function did not return successfully (0): ret %d",
				ret);
			zassert_equal(reconfiguration.sample_rate_hz,
				      reconfiguration.sample_rate_hz,
				      "Failed to reconfigure module");
			zassert_equal(reconfiguration.bits_per_sample,
				      reconfiguration.bits_per_sample,
				      "Failed to reconfigure module");
			zassert_equal(reconfiguration.carried_bits_per_sample,
				      reconfiguration.carried_bits_per_sample,
				      "Failed to reconfigure module");
			zassert_equal(reconfiguration.amplitude, reconfiguration.amplitude,
				      "Failed to reconfigure module");
			zassert_equal(reconfiguration.interleaved, reconfiguration.interleaved,
				      "Failed to reconfigure module");

			ret = audio_module_start(&handle[0]);
			zassert_equal(ret, 0,
				      "Start function did not return successfully (0): ret %d",
				      ret);
			zassert_equal(handle[0].state, AUDIO_MODULE_STATE_RUNNING,
				      "Not in running state after call to start the audio module");
		}
	}

	ret = audio_module_disconnect(&handle[0], NULL, true);
	zassert_equal(ret, 0, "Disconnect function did not return successfully (0): ret %d", ret);
	zassert_equal(handle[0].use_tx_queue, 0, "Disconnect queue size is not %d, but is %d", 0,
		      handle[0].use_tx_queue);

	ret = audio_module_stop(&handle[0]);
	zassert_equal(ret, 0, "Stop function did not return successfully (0): ret %d", ret);
	zassert_equal(handle[0].state, AUDIO_MODULE_STATE_STOPPED,
		      "Not in stopped state after call to stop the audio module");

	ret = audio_module_close(&handle[0]);
	zassert_equal(ret, 0, "Close function did not return successfully (0): ret %d", ret);
}
