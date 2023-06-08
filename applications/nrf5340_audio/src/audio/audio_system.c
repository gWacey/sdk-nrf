/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_system.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/audio/audio.h>

#include "macros_common.h"
#include "sw_codec_select.h"
#include "audio_datapath.h"
#include "audio_i2s.h"
#include "data_fifo.h"
#include "hw_codec.h"
#include "tone.h"
#include "contin_array.h"
#include "pcm_stream_channel_modifier.h"
#include "audio_usb.h"
#include "streamctrl.h"
#include "amod_api.h"
#include "lc3_decoder.h"
#include "modules.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_system, CONFIG_AUDIO_SYSTEM_LOG_LEVEL);

#define FIFO_TX_BLOCK_COUNT (CONFIG_FIFO_FRAME_SPLIT_NUM * CONFIG_FIFO_TX_FRAME_COUNT)
#define FIFO_RX_BLOCK_COUNT (CONFIG_FIFO_FRAME_SPLIT_NUM * CONFIG_FIFO_RX_FRAME_COUNT)

#define DEBUG_INTERVAL_NUM 1000

/* Allocate audio memory */
K_THREAD_STACK_DEFINE(lc3_dec1_thread_stack, CONFIG_ENCODER_STACK_SIZE);
DATA_FIFO_DEFINE(lc3_dec1_fifo_tx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(lc3_dec1_fifo_rx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(lc3_enc1_fifo_tx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
DATA_FIFO_DEFINE(lc3_enc1_fifo_rx, CONFIG_DECODER_MSG_QUEUE_SIZE, sizeof(struct amod_message));
K_MEM_SLAB_DEFINE(audio_pcm_data_slab, FRAME_SIZE_BYTES, CONFIG_PCM_DATA_OBJECTS_NUM, 4);
K_MEM_SLAB_DEFINE(audio_coded_data_slab, FRAME_SIZE_BYTES, CONFIG_CODED_DATA_OBJECTS_NUM, 4);

static struct amod_handle handles[MODULES_ID_NUM];
static struct lc3_decoder_context decoder_context[LC3_DECODER_ID_NUM];

static struct sw_codec_config sw_codec_cfg;
/* Buffer which can hold max 1 period test tone at 1000 Hz */
static int16_t test_tone_buf[CONFIG_AUDIO_SAMPLE_RATE_HZ / 1000];
static size_t test_tone_size;

static void audio_gateway_configure(void)
{
	if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.sw_codec = SW_CODEC_LC3;
	} else {
		ERR_CHK_MSG(-EINVAL, "No codec selected");
	}

#if (CONFIG_STREAM_BIDIRECTIONAL)
	sw_codec_cfg.decoder.enabled = true;
	sw_codec_cfg.decoder.num_ch = SW_CODEC_MONO;
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

	if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.encoder.bitrate = CONFIG_LC3_BITRATE;
	} else {
		ERR_CHK_MSG(-EINVAL, "No codec selected");
	}

	if (IS_ENABLED(CONFIG_MONO_TO_ALL_RECEIVERS)) {
		sw_codec_cfg.encoder.num_ch = SW_CODEC_MONO;
	} else {
		sw_codec_cfg.encoder.num_ch = SW_CODEC_STEREO;
	}

	sw_codec_cfg.encoder.enabled = true;
}

static void audio_headset_configure(void)
{
	if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.sw_codec = SW_CODEC_LC3;
	} else {
		ERR_CHK_MSG(-EINVAL, "No codec selected");
	}

#if (CONFIG_STREAM_BIDIRECTIONAL)
	sw_codec_cfg.encoder.enabled = true;
	sw_codec_cfg.encoder.num_ch = SW_CODEC_MONO;

	if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.encoder.bitrate = CONFIG_LC3_BITRATE;
	} else {
		ERR_CHK_MSG(-EINVAL, "No codec selected");
	}
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

	sw_codec_cfg.decoder.num_ch = SW_CODEC_MONO;
	sw_codec_cfg.decoder.enabled = true;
}

int audio_encode_test_tone_set(uint32_t freq)
{
	int ret;

	if (freq == 0) {
		test_tone_size = 0;
		return 0;
	}

	ret = tone_gen(test_tone_buf, &test_tone_size, freq, CONFIG_AUDIO_SAMPLE_RATE_HZ, 1);
	ERR_CHK(ret);

	if (test_tone_size > sizeof(test_tone_buf)) {
		return -ENOMEM;
	}

	return 0;
}

/**@brief Initializes the FIFOs, the codec, and starts the I2S
 */
void audio_system_start(void)
{
	int ret;
	int i, j;
	struct lc3_decoder_configuration lc3_dec_initial_config = {
		.sample_rate = CONFIG_AUDIO_SAMPLE_RATE_HZ,
		.bit_depth = CONFIG_AUDIO_BIT_DEPTH_BITS,
		.duration_us = CONFIG_AUDIO_FRAME_DURATION_US,
		.number_channels = sw_codec_cfg.decoder.num_ch,
		.channel_map = (BT_AUDIO_LOCATION_FRONT_LEFT || BT_AUDIO_LOCATION_FRONT_RIGHT)};

	struct lc3_decoder_configuration lc3_enc_initial_config = {
		.sample_rate = CONFIG_AUDIO_SAMPLE_RATE_HZ,
		.bit_depth = CONFIG_AUDIO_BIT_DEPTH_BITS,
		.duration_us = CONFIG_AUDIO_FRAME_DURATION_US,
		.number_channels = sw_codec_cfg.encoder.num_ch,
		.channel_map = (BT_AUDIO_LOCATION_FRONT_LEFT || BT_AUDIO_LOCATION_FRONT_RIGHT)};

	struct amod_table modules[CONFIG_MODULES_NUM] = {
		{/* LC3 decoder 1 */
		 .name = "lc3 dec 1",
		 .handle = &handles[MODULE_ID_1],
		 .initial_config = (struct amod_configuration *)&lc3_dec_initial_config,
		 .params.description = lc3_dec_description,
		 .params.thread.stack = lc3_dec1_thread_stack,
		 .params.thread.stack_size = CONFIG_DECODER_STACK_SIZE,
		 .params.thread.priority = CONFIG_DECODER_THREAD_PRIO,
		 .params.thread.msg_rx = &lc3_dec1_fifo_rx,
		 .params.thread.msg_tx = &lc3_dec1_fifo_tx,
		 .params.thread.data_slab = &audio_coded_data_slab,
		 .params.thread.data_size = FRAME_SIZE_BYTES,
		 .context = (struct amod_context *)&decoder_context[LC3_DECODER_ID_1]},

		{/* LC3 encoder 1 */
		 .name = "lc3 enc 1",
		 .handle = &handles[MODULE_ID_2],
		 .initial_config = (struct amod_configuration *)&lc3_enc_initial_config,
		 .params.description = lc3_enc_description,
		 .params.thread.stack = lc3_enc1_thread_stack,
		 .params.thread.stack_size = CONFIG_ENCODER_STACK_SIZE,
		 .params.thread.priority = CONFIG_ENCODER_THREAD_PRIO,
		 .params.thread.msg_rx = &lc3_enc1_fifo_rx,
		 .params.thread.msg_tx = &lc3_enc1_fifo_tx,
		 .params.thread.data_slab = &audio_pcm_data_slab,
		 .params.thread.data_size = FRAME_SIZE_BYTES,
		 .context = (struct amod_context *)&decoder_context[LC3_ENCODER_ID_1]}};
	bool connections[CONFIG_MODULES_NUM][CONFIG_MODULES_NUM] = {
		/* Decoder 1 */ {true, false, true},
		/* Encoder 1 */ {false, true, false},
		/* I2S I/O   */ {false, true, false}};

	if (CONFIG_AUDIO_DEV == HEADSET) {
		audio_headset_configure();
	} else if (CONFIG_AUDIO_DEV == GATEWAY) {
		audio_gateway_configure();
	} else {
		LOG_ERR("Invalid CONFIG_AUDIO_DEV: %d", CONFIG_AUDIO_DEV);
		ERR_CHK(-EINVAL);
	}

	if (!fifo_tx.initialized) {
		ret = data_fifo_init(&fifo_tx);
		ERR_CHK_MSG(ret, "Failed to set up tx FIFO");
	}

	if (!fifo_rx.initialized) {
		ret = data_fifo_init(&fifo_rx);
		ERR_CHK_MSG(ret, "Failed to set up rx FIFO");
	}

	/* Audio module(s) configure */
	for (i = 0; i < CONFIG_MODULES_NUM; i++) {
		ret = amod_open(&modules[i].params, modules[i].initial_config, modules[i].name,
				modules[i].handle, modules[i].context);
		if (ret) {
			LOG_DBG("Failed to open the decoder module, ret %d", ret);
			return ret;
		}

		ret = amod_configuration_set(modules[i].handle, modules[i].initial_config);
		if (ret) {
			LOG_DBG("Failed to configure the decoder module, ret %d", ret);
			return ret;
		}
	}

	/* Connect the stream(s) */
	for (j = 0; j < CONFIG_MODULES_NUM; j++) {
		for (i = 0; i < CONFIG_MODULES_NUM; i++) {
			if (connections[j][i]) {
				ret = amod_connect(modules[j].handle, modules[i].handle);
			}
		}
	}

	if (sw_codec_cfg.encoder.enabled && encoder_thread_id == NULL) {
		encoder_thread_id = k_thread_create(
			&encoder_thread_data, encoder_thread_stack, CONFIG_ENCODER_STACK_SIZE,
			(k_thread_entry_t)encoder_thread, NULL, NULL, NULL,
			K_PRIO_PREEMPT(CONFIG_ENCODER_THREAD_PRIO), 0, K_NO_WAIT);
		ret = k_thread_name_set(encoder_thread_id, "ENCODER");
		ERR_CHK(ret);
	}

#if ((CONFIG_AUDIO_SOURCE_USB) && (CONFIG_AUDIO_DEV == GATEWAY))
	ret = audio_usb_start(&fifo_tx, &fifo_rx);
	ERR_CHK(ret);
#else
	ret = hw_codec_default_conf_enable();
	ERR_CHK(ret);

	ret = audio_datapath_start(&fifo_rx);
	ERR_CHK(ret);
#endif /* ((CONFIG_AUDIO_SOURCE_USB) && (CONFIG_AUDIO_DEV == GATEWAY))) */
}

void audio_system_stop(void)
{
	int ret;

	if (!sw_codec_cfg.initialized) {
		LOG_WRN("Codec already unitialized");
		return;
	}

	LOG_DBG("Stopping codec");

#if ((CONFIG_AUDIO_DEV == GATEWAY) && CONFIG_AUDIO_SOURCE_USB)
	audio_usb_stop();
#else
	ret = hw_codec_soft_reset();
	ERR_CHK(ret);

	ret = audio_datapath_stop();
	ERR_CHK(ret);
#endif /* ((CONFIG_AUDIO_DEV == GATEWAY) && CONFIG_AUDIO_SOURCE_USB) */

	for (int i = 0; i < CONFIG_MODULES_NUM; i++) {
		ret = amod_stop(&handles[i]);
		if (ret) {
			LOG_WRN("Module failed of stop, ret %d", ret);
		}

		ret = amod_close(&handles[i]);
		if (ret) {
			LOG_WRN("Module may not have closed, ret %d", ret);
		}
	}

	sw_codec_cfg.initialized = false;

	data_fifo_empty(&lc3_dec1_fifo_tx);
	data_fifo_empty(&lc3_dec1_fifo_rx);
}

int audio_system_fifo_rx_block_drop(void)
{
	int ret;
	void *temp;
	size_t temp_size;

	ret = data_fifo_pointer_last_filled_get(&fifo_rx, &temp, &temp_size, K_NO_WAIT);
	if (ret) {
		LOG_WRN("Failed to get last filled block");
		return -ECANCELED;
	}

	data_fifo_block_free(&fifo_rx, &temp);

	LOG_DBG("Block dropped");
	return 0;
}

void audio_system_init(void)
{
	int ret;

#if ((CONFIG_AUDIO_DEV == GATEWAY) && (CONFIG_AUDIO_SOURCE_USB))
	ret = audio_usb_init();
	ERR_CHK(ret);
#else
	ret = audio_datapath_init();
	ERR_CHK(ret);
	ret = hw_codec_init();
	ERR_CHK(ret);
#endif
}

static int cmd_audio_system_start(const struct shell *shell, size_t argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	audio_system_start();

	shell_print(shell, "Audio system started");

	return 0;
}

static int cmd_audio_system_stop(const struct shell *shell, size_t argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	audio_system_stop();

	shell_print(shell, "Audio system stopped");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(audio_system_cmd,
			       SHELL_COND_CMD(CONFIG_SHELL, start, NULL, "Start the audio system",
					      cmd_audio_system_start),
			       SHELL_COND_CMD(CONFIG_SHELL, stop, NULL, "Stop the audio system",
					      cmd_audio_system_stop),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(audio_system, &audio_system_cmd, "Audio system commands", NULL);
