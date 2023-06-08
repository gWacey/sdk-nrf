/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "audio_i2s_io.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "aobj_api.h"
#include "amod_api.h"
#include "audio_sync.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_i2s_io, 4); /* CONFIG_AUDIO_I2S_IO_LOG_LEVEL); */

/*
 * Terminology
 *   - sample: signed integer of audio waveform amplitude
 *   - sample FIFO: circular array of raw audio samples
 *   - block: set of raw audio samples exchanged with I2S
 *   - frame: encoded audio packet exchanged with connectivity
 */

#define BLK_PERIOD_US 1000

/* Total sample FIFO period in microseconds */
#define FIFO_SMPL_PERIOD_US (CONFIG_AUDIO_MAX_PRES_DLY_US * 2)
#define FIFO_NUM_BLKS NUM_BLKS(FIFO_SMPL_PERIOD_US)
#define MAX_FIFO_SIZE (FIFO_NUM_BLKS * BLK_SIZE_SAMPLES(CONFIG_AUDIO_SAMPLE_RATE_HZ) * 2)

/* Number of audio blocks given a duration */
#define NUM_BLKS(d) ((d) / BLK_PERIOD_US)
/* Single audio block size in number of samples (stereo) */
#define BLK_SIZE_SAMPLES(r) (((r)*BLK_PERIOD_US) / 1000000)
/* Increment sample FIFO index by one block */
#define NEXT_IDX(i) (((i) < (FIFO_NUM_BLKS - 1)) ? ((i) + 1) : 0)
/* Decrement sample FIFO index by one block */
#define PREV_IDX(i) (((i) > 0) ? ((i)-1) : (FIFO_NUM_BLKS - 1))

#define NUM_BLKS_IN_FRAME NUM_BLKS(CONFIG_AUDIO_FRAME_DURATION_US)
#define BLK_MONO_NUM_SAMPS BLK_SIZE_SAMPLES(CONFIG_AUDIO_SAMPLE_RATE_HZ)
#define BLK_STEREO_NUM_SAMPS (BLK_MONO_NUM_SAMPS * 2)
/* Number of octets in a single audio block */
#define BLK_MONO_SIZE_OCTETS (BLK_MONO_NUM_SAMPS * CONFIG_AUDIO_BIT_DEPTH_OCTETS)
#define BLK_STEREO_SIZE_OCTETS (BLK_MONO_SIZE_OCTETS * 2)

/* 3000 us to allow BLE transmission and (host -> HCI -> controller) */
#define JUST_IN_TIME_US (CONFIG_AUDIO_FRAME_DURATION_US - 3000)
#define JUST_IN_TIME_THRESHOLD_US 1500

/* How often to print underrun warning */
#define UNDERRUN_LOG_INTERVAL_BLKS 5000

static struct {
	bool datapath_initialized;
	bool stream_started;
	void *decoded_data;

	struct {
		struct data_fifo *fifo;
	} in;

	struct {
#if CONFIG_AUDIO_BIT_DEPTH_16
		int16_t __aligned(sizeof(uint32_t)) fifo[MAX_FIFO_SIZE];
#elif CONFIG_AUDIO_BIT_DEPTH_32
		int32_t __aligned(sizeof(uint32_t)) fifo[MAX_FIFO_SIZE];
#endif
		uint16_t prod_blk_idx; /* Output producer audio block index */
		uint16_t cons_blk_idx; /* Output consumer audio block index */
		uint32_t prod_blk_ts[FIFO_NUM_BLKS];
		/* Statistics */
		uint32_t total_blk_underruns;
	} out;

	struct sync sync_ctx;
} ctrl_blk;

/**
 * @brief Table of the I2S module functions.
 *
 */
struct amod_functions audio_i2s_io_functions = {
	/**
	 * @brief  Function to an open the I2S module.
	 */
	.open = audio_i2s_io_open,

	/**
	 * @brief  Function to close the I2S module.
	 */
	.close = audio_i2s_io_close,

	/**
	 * @brief  Function to set the configuration of the I2S module.
	 */
	.configuration_set = audio_i2s_io_configuration_set,

	/**
	 * @brief  Function to get the configuration of the I2S module.
	 */
	.configuration_get = audio_i2s_io_configuration_get,

	/**
	 * @brief Start a module processing data.
	 *
	 */
	.start = audio_i2s_io_start,

	/**
	 * @brief Pause a module processing data.
	 *
	 */
	.pause = audio_i2s_io_pause,

	/**
	 * @brief The core data processing function in the I2S module.
	 */
	.data_process = audio_i2s_io_data_process,
};

/**
 * @brief The set-up parameters for the I2S.
 *
 */
struct amod_description audio_i2s_io_dept = {
	.name = "Audio I2S I/O",
	.type = AMOD_TYPE_PROCESSOR,
	.functions = (struct amod_functions *)&audio_i2s_io_functions
};

/**
 * @brief A private pointer to the I2S set-up parameters.
 *
 */
struct amod_description *audio_i2s_io_description = &audio_i2s_io_dept;

/**
 * @brief Start I2S.
 *
 */
static void i2s_io_start(struct audio_i2s_io_context *ctx)
{
	int ret;

	/* Double buffer I2S */
	uint8_t *tx_buf_one = NULL;
	uint8_t *tx_buf_two = NULL;
	uint32_t *rx_buf_one = NULL;
	uint32_t *rx_buf_two = NULL;

	/* TX */
	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || (CONFIG_AUDIO_DEV == HEADSET)) {
		ctrl_blk.out.cons_blk_idx = PREV_IDX(ctrl_blk.out.cons_blk_idx);
		tx_buf_one = (uint8_t *)&ctrl_blk.out
				     .fifo[ctrl_blk.out.cons_blk_idx * BLK_STEREO_NUM_SAMPS];

		ctrl_blk.out.cons_blk_idx = PREV_IDX(ctrl_blk.out.cons_blk_idx);
		tx_buf_two = (uint8_t *)&ctrl_blk.out
				     .fifo[ctrl_blk.out.cons_blk_idx * BLK_STEREO_NUM_SAMPS];
	}

	/* RX */
	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || (CONFIG_AUDIO_DEV == GATEWAY)) {
		uint32_t alloced_cnt;
		uint32_t locked_cnt;

		ret = data_fifo_num_used_get(ctrl_blk.in.fifo, &alloced_cnt, &locked_cnt);
		if (alloced_cnt || locked_cnt || ret) {
			ERR_CHK_MSG(-ENOMEM, "Fifo is not empty!");
		}

		ret = data_fifo_pointer_first_vacant_get(ctrl_blk.in.fifo, (void **)&rx_buf_one,
							 K_NO_WAIT);
		ERR_CHK_MSG(ret, "RX failed to get block");
		ret = data_fifo_pointer_first_vacant_get(ctrl_blk.in.fifo, (void **)&rx_buf_two,
							 K_NO_WAIT);
		ERR_CHK_MSG(ret, "RX failed to get block");
	}

	/* Start I2S */
	audio_i2s_start(tx_buf_one, rx_buf_one);
	audio_i2s_io_set_next_buf(tx_buf_two, rx_buf_two);
}

/**
 * @brief Get first available alternative-buffer
 *
 * @param p_buffer Double pointer to populate with buffer
 *
 * @retval 0 if success
 * @retval -ENOMEM No available buffers
 */
static int alt_buffer_get(void **p_buffer)
{
	if (!alt.buf_0_in_use) {
		alt.buf_0_in_use = true;
		*p_buffer = alt.buf_0;
	} else if (!alt.buf_1_in_use) {
		alt.buf_1_in_use = true;
		*p_buffer = alt.buf_1;
	} else {
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Checks if pointer matches that of a buffer
 *	      and frees it in one operation
 *
 * @param p_buffer Buffer to free
 */
static void alt_buffer_free(void const *const p_buffer)
{
	if (p_buffer == alt.buf_0) {
		alt.buf_0_in_use = false;
	} else if (p_buffer == alt.buf_1) {
		alt.buf_1_in_use = false;
	}
}

/**
 * @brief Frees both alternative buffers
 */
static void alt_buffer_free_both(void)
{
	alt.buf_0_in_use = false;
	alt.buf_1_in_use = false;
}

/*
 * This handler function is called every time I2S needs new buffers for
 * TX and RX data.
 *
 * The new TX data buffer is the next consumer block in out.fifo.
 *
 * The new RX data buffer is the first empty slot of in.fifo.
 * New I2S RX data is located in rx_buf_released, and is locked into
 * the in.fifo message queue.
 */
static void audio_datapath_i2s_blk_complete(uint32_t frame_start_ts, uint32_t *rx_buf_released,
					    uint32_t const *tx_buf_released)
{
	int ret;
	static bool underrun_condition;

	alt_buffer_free(tx_buf_released);

	/*** Presentation delay measurement ***/
	ctrl_blk.current_pres_dly_us =
		frame_start_ts - ctrl_blk.out.prod_blk_ts[ctrl_blk.out.cons_blk_idx];

	/********** I2S TX **********/
	static uint8_t *tx_buf;

	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || (CONFIG_AUDIO_DEV == HEADSET)) {
		if (tx_buf_released != NULL) {
			/* Double buffered index */
			uint32_t next_out_blk_idx = NEXT_IDX(ctrl_blk.out.cons_blk_idx);

			if (next_out_blk_idx != ctrl_blk.out.prod_blk_idx) {
				/* Only increment if not in underrun condition */
				ctrl_blk.out.cons_blk_idx = next_out_blk_idx;
				if (underrun_condition) {
					underrun_condition = false;
					LOG_WRN("Data received, total underruns: %d",
						ctrl_blk.out.total_blk_underruns);
				}

				tx_buf = (uint8_t *)&ctrl_blk.out
						 .fifo[next_out_blk_idx * BLK_STEREO_NUM_SAMPS];

			} else {
				if (stream_state_get() == STATE_STREAMING) {
					underrun_condition = true;
					ctrl_blk.out.total_blk_underruns++;

					if ((ctrl_blk.out.total_blk_underruns %
					     UNDERRUN_LOG_INTERVAL_BLKS) == 0) {
						LOG_WRN("In I2S TX underrun condition, total: %d",
							ctrl_blk.out.total_blk_underruns);
					}
				}

				/*
				 * No data available in out.fifo
				 * use alternative buffers
				 */
				ret = alt_buffer_get((void **)&tx_buf);
				ERR_CHK(ret);

				memset(tx_buf, 0, BLK_STEREO_SIZE_OCTETS);
			}

			if (tone_active) {
				tone_mix(tx_buf);
			}
		}
	}

	/********** I2S RX **********/
	static uint32_t *rx_buf;
	static int prev_ret;

	if (IS_ENABLED(CONFIG_STREAM_BIDIRECTIONAL) || (CONFIG_AUDIO_DEV == GATEWAY)) {
		/* Lock last filled buffer into message queue */
		if (rx_buf_released != NULL) {
			ret = data_fifo_block_lock(ctrl_blk.in.fifo, (void **)&rx_buf_released,
						   BLOCK_SIZE_BYTES);

			ERR_CHK_MSG(ret, "Unable to lock block RX");
		}

		/* Get new empty buffer to send to I2S HW */
		ret = data_fifo_pointer_first_vacant_get(ctrl_blk.in.fifo, (void **)&rx_buf,
							 K_NO_WAIT);
		if (ret == 0 && prev_ret == -ENOMEM) {
			LOG_WRN("I2S RX continuing stream");
			prev_ret = ret;
		}

		/* If RX FIFO is filled up */
		if (ret == -ENOMEM) {
			void *data;
			size_t size;

			if (ret != prev_ret) {
				LOG_WRN("I2S RX overrun. Single msg");
				prev_ret = ret;
			}

			ret = data_fifo_pointer_last_filled_get(ctrl_blk.in.fifo, &data, &size,
								K_NO_WAIT);
			ERR_CHK(ret);

			data_fifo_block_free(ctrl_blk.in.fifo, &data);

			ret = data_fifo_pointer_first_vacant_get(ctrl_blk.in.fifo, (void **)&rx_buf,
								 K_NO_WAIT);
		}

		ERR_CHK_MSG(ret, "RX failed to get block");
	}

	/*** Data exchange ***/
	audio_i2s_set_next_buf(tx_buf, rx_buf);

	/*** Drift compensation ***/
	if (ctrl_blk.drift_comp.enabled) {
		audio_sync_drift_compensation(frame_start_ts);
	}
}

/**
 * @brief Open an instance of the I2S
 *
 */
int audio_i2s_io_open(struct handle *handle, struct amod_configuration *configuration)
{
	struct audio_i2s_io_configuration *config =
		(struct audio_i2s_io_configuration *)configuration;
	int ret;

	memset(&ctrl_blk, 0, sizeof(ctrl_blk));
	audio_i2s_blk_comp_cb_register(audio_datapath_i2s_blk_complete);
	audio_i2s_init(ctx);
	ctrl_blk.datapath_initialized = true;
	ctrl_blk.drift_comp.enabled = true;
	ctrl_blk.pres_comp.enabled = true;
	ctrl_blk.pres_comp.pres_delay_us = CONFIG_BT_AUDIO_PRESENTATION_DELAY_US;

	return 0;
}

/**
 * @brief  Function close an instance of the I2S.
 *
 */
int audio_i2s_io_close(struct handle *handle)
{
	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the I2S.
 *
 */
int audio_i2s_io_configuration_set(struct handle *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct audio_i2s_io_context *ctx = (struct audio_i2s_io_context *)hdl->context;
	struct audio_i2s_io_configuration *config =
		(struct audio_i2s_io_configuration *)configuration;

	memcpy(&ctx->config, config, sizeof(struct audio_i2s_io_configuration));

	return 0;
}

/**
 * @brief  Function to set the configuration of an instance of the I2S.
 *
 */
int audio_i2s_io_configuration_get(struct handle *handle, struct amod_configuration *configuration)
{
	struct amod_handle *hdl = (struct amod_handle *)handle;
	struct audio_i2s_io_context *ctx = (struct audio_i2s_io_context *)hdl->context;
	struct audio_i2s_io_configuration *config =
		(struct audio_i2s_io_configuration *)configuration;

	memcpy(config, &ctx->config, sizeof(struct audio_i2s_io_configuration));

	return 0;
}

/**
 * @brief  Function to start the I2S processing data.
 *
 */
int audio_i2s_io_start(struct handle *handle)
{
	if (!ctrl_blk.datapath_initialized) {
		LOG_WRN("Audio datapath not initialized");
		return -ECANCELED;
	}

	if (!ctrl_blk.stream_started) {
		ctrl_blk.in.fifo = fifo_rx;

		/* Clear counters and mute initial audio */
		memset(&ctrl_blk.out, 0, sizeof(ctrl_blk.out));

		i2s_io_start(ctx);
		ctrl_blk.stream_started = true;

		return 0;
	} else {
		return -EALREADY;
	}
}

static void i2s_io_stop(struct alternate *alt)
{
	audio_i2s_stop();
	alt_buffer_free_both(alt);
}

/**
 * @brief  Function to stop the I2S processing data.
 *
 */
int audio_i2s_io_pause(struct handle *handle)
{
	if (ctrl_blk.stream_started) {
		ctrl_blk.stream_started = false;
		i2s_io_stop(&alt);
		ctrl_blk.sync_ctx.previous_sdu_ref_us = 0;

		pres_comp_state_set(PRES_STATE_INIT);

		return 0;
	} else {
		return -EALREADY;
	}
}
/**
 * @brief Process an audio data block in an instance of the I2S.
 *
 */
int audio_i2s_io_data_process(struct handle *handle, struct aobj_block *block_in,
			      struct aobj_block *block_out)
{
	if (!ctrl_blk.stream_started) {
		LOG_WRN("Stream not started");
		return;
	}

	/*** Check incoming data ***/
	if (!block_in->data || block_in->data_type != AOBJ_TYPE_PCM) {
		LOG_DBG("Error in input data for module %s", hdl->name);
		return -EINVAL;
	}

	if (block_in->reference_ts == ctrl_blk.sync_ctx.previous_sdu_ref_us) {
		LOG_WRN("Duplicate sdu_ref_us (%d) - Dropping audio frame", block_in->reference_ts);
		return;
	}

	if (block_in->bad_frame) {
		/* Error in the frame or frame lost - sdu_ref_us is stil valid */
		LOG_DBG("Bad audio frame");
	}

	/* Split decoded frame into CONFIG_FIFO_FRAME_SPLIT_NUM blocks */
	for (int i = 0; i < CONFIG_FIFO_FRAME_SPLIT_NUM; i++) {
		memcpy(tmp_pcm_raw_data[i], (char *)pcm_raw_data + (i * (BLOCK_SIZE_BYTES)),
		       BLOCK_SIZE_BYTES);

		ret = data_fifo_block_lock(&fifo_tx, &tmp_pcm_raw_data[i], BLOCK_SIZE_BYTES);
		if (ret) {
			LOG_ERR("Failed to lock block");
			return ret;
		}
	}

	bool sdu_ref_not_consecutive = false;

	if (ctrl_blk.sync_ctx.previous_sdu_ref_us) {
		uint32_t sdu_ref_delta_us =
			block_in->reference_ts - ctrl_blk.sync_ctx.previous_sdu_ref_us;

		/* Check if the delta is from two consecutive frames */
		if (sdu_ref_delta_us <
		    (CONFIG_AUDIO_FRAME_DURATION_US + (CONFIG_AUDIO_FRAME_DURATION_US / 2))) {
			/* Check for invalid delta */
			if ((sdu_ref_delta_us >
			     (CONFIG_AUDIO_FRAME_DURATION_US + SDU_REF_DELTA_MAX_ERR_US)) ||
			    (sdu_ref_delta_us <
			     (CONFIG_AUDIO_FRAME_DURATION_US - SDU_REF_DELTA_MAX_ERR_US))) {
				LOG_DBG("Invalid sdu_ref_us delta (%d) - Estimating sdu_ref_us",
					sdu_ref_delta_us);

				/* Estimate sdu_ref_us */
				block_in->reference_ts = ctrl_blk.sync_ctx.previous_sdu_ref_us +
							 CONFIG_AUDIO_FRAME_DURATION_US;
			}
		} else {
			LOG_INF("sdu_ref_us not from consecutive frames (diff: %d us)",
				block_in->reference_ts);
			sdu_ref_not_consecutive = true;
		}
	}

	ctrl_blk.sync_ctx.previous_sdu_ref_us = block_in->reference_ts;

	/*** Presentation compensation ***/
	if (ctrl_blk.pres_comp.enabled) {
		audio_sync_presentation_compensation(&ctrl_blk.sync_ctx, block_in->block_rx_ts,
						     block_in->reference_ts,
						     sdu_ref_not_consecutive);
	}

	/*** Add audio data to FIFO buffer ***/

	ctrl_blk.decoded_data = block_in->data;

	int32_t num_blks_in_fifo = ctrl_blk.out.prod_blk_idx - ctrl_blk.out.cons_blk_idx;

	if ((num_blks_in_fifo + NUM_BLKS_IN_FRAME) > FIFO_NUM_BLKS) {
		LOG_WRN("Output audio stream overrun - Discarding audio frame");

		/* Discard frame to allow consumer to catch up */
		return;
	}

	uint32_t out_blk_idx = ctrl_blk.out.prod_blk_idx;

	/* Split the input block into 1ms sub blocks */
	for (uint32_t i = 0; i < NUM_BLKS_IN_FRAME; i++) {
		if (IS_ENABLED(CONFIG_AUDIO_BIT_DEPTH_16)) {
			memcpy(&ctrl_blk.out.fifo[out_blk_idx * BLK_STEREO_NUM_SAMPS],
			       &((int16_t *)ctrl_blk.decoded_data)[i * BLK_STEREO_NUM_SAMPS],
			       BLK_STEREO_SIZE_OCTETS);
		} else if (IS_ENABLED(CONFIG_AUDIO_BIT_DEPTH_32)) {
			memcpy(&ctrl_blk.out.fifo[out_blk_idx * BLK_STEREO_NUM_SAMPS],
			       &((int32_t *)ctrl_blk.decoded_data)[i * BLK_STEREO_NUM_SAMPS],
			       BLK_STEREO_SIZE_OCTETS);
		}

		/* Record producer block start reference */
		ctrl_blk.out.prod_blk_ts[out_blk_idx] = block_in->block_rx_ts + (i * BLK_PERIOD_US);

		out_blk_idx = NEXT_IDX(out_blk_idx);
	}

	ctrl_blk.out.prod_blk_idx = out_blk_idx;

	return 0;
}
