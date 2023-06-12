/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "audio_sync.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <errno.h>
#include "aobj_api.h"
#include "amod_api.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_sync, 4); /* CONFIG_AUDIO_SYNC_LOG_LEVEL); */

static const char *const drift_comp_state_names[] = {
	"INIT",
	"CALIB",
	"OFFSET",
	"LOCKED",
};

static const char *const pres_comp_state_names[] = {
	"INIT",
	"MEAS",
	"WAIT",
	"LOCKED",
};

/* How many function calls before moving on with drift compensation */
#define DRIFT_COMP_WAITING_CNT (DRIFT_MEAS_PERIOD_US / BLK_PERIOD_US)
/* How much data to be collected before moving on with presentation compensation */
#define PRES_COMP_NUM_DATA_PTS (DRIFT_MEAS_PERIOD_US / CONFIG_AUDIO_FRAME_DURATION_US)

/* Audio clock - nRF5340 Analog Phase-Locked Loop (APLL) */
#define APLL_FREQ_CENTER 39854
#define APLL_FREQ_MIN 36834
#define APLL_FREQ_MAX 42874
/* Use nanoseconds to reduce rounding errors */
#define APLL_FREQ_ADJ(t) (-((t)*1000) / 331)

#define DRIFT_MEAS_PERIOD_US 100000
#define DRIFT_ERR_THRESH_LOCK 16
#define DRIFT_ERR_THRESH_UNLOCK 32

static void drift_comp_state_set(struct sync *sync_ctx, enum drift_comp_state new_state)
{
	if (new_state == sync_ctx->drift_comp.state) {
		LOG_WRN("Trying to change to the same drift compensation state");
		return;
	}

	sync_ctx->drift_comp.ctr = 0;

	sync_ctx->drift_comp.state = new_state;
	LOG_INF("Drft comp state: %s", drift_comp_state_names[new_state]);
}

/**
 * @brief Adjust timing to make sure audio data is sent just in time for BLE event
 *
 * @note  The time from last anchor point is checked and then blocks of 1ms
 *        can be dropped to allow the sending of encoded data to be sent just
 *        before the connection interval opens up. This is done to reduce overall
 *        latency.
 *
 * @param[in]  sdu_ref_us  The SDU reference, in µs, to the previous sent packet
 */
static void audio_sync_just_in_time_check_and_adjust(uint32_t sdu_ref_us)
{
	static int32_t count;
	int ret;

	uint32_t curr_frame_ts = nrfx_timer_capture(&audio_sync_timer_instance,
						    AUDIO_SYNC_TIMER_CURR_TIME_CAPTURE_CHANNEL);
	int diff = curr_frame_ts - sdu_ref_us;

	if (count++ % 100 == 0) {
		LOG_DBG("Time from last anchor: %d", diff);
	}

	if (diff < JUST_IN_TIME_US - JUST_IN_TIME_THRESHOLD_US ||
	    diff > JUST_IN_TIME_US + JUST_IN_TIME_THRESHOLD_US) {
		ret = audio_system_fifo_rx_block_drop();
		if (ret) {
			LOG_WRN("Not able to drop FIFO RX block");
			return;
		}

		count = 0;
	}
}

int audio_sync_pres_delay_us_set(uint32_t delay_us)
{
	if (!IN_RANGE(delay_us, CONFIG_AUDIO_MIN_PRES_DLY_US, CONFIG_AUDIO_MAX_PRES_DLY_US)) {
		LOG_WRN("Presentation delay not supported: %d", delay_us);
		return -EINVAL;
	}

	ctrl_blk.pres_comp.pres_delay_us = delay_us;

	LOG_DBG("Presentation delay set to %d us", delay_us);

	return 0;
}

void audio_sync_pres_delay_us_get(uint32_t *delay_us)
{
	*delay_us = ctrl_blk.pres_comp.pres_delay_us;
}

void audio_sync_sdu_ref_update(uint32_t sdu_ref_us, bool adjust)
{
	if (IS_ENABLED(CONFIG_AUDIO_SOURCE_I2S)) {
		if (ctrl_blk.stream_started) {
			ctrl_blk.previous_sdu_ref_us = sdu_ref_us;

			if (adjust) {
				audio_sync_just_in_time_check_and_adjust(sdu_ref_us);
			}
		} else {
			LOG_WRN("Stream not startet - Can not update sdu_ref_us");
		}
	}
}

/**
 * @brief Adjust frequency of HFCLKAUDIO to get audio in sync
 *
 */
void audio_sync_drift_compensation(struct sync *sync_ctx, uint32_t frame_start_ts)
{
	switch (sync_ctx->drift_comp.state) {
	case DRIFT_STATE_INIT: {
		/* Check if audio data has been received */
		if (sync_ctx->previous_sdu_ref_us) {
			sync_ctx->drift_comp.meas_start_time_us = sync_ctx->previous_sdu_ref_us;

			drift_comp_state_set(DRIFT_STATE_CALIB);
		}
		break;
	}
	case DRIFT_STATE_CALIB: {
		if (++sync_ctx->drift_comp.ctr < DRIFT_COMP_WAITING_CNT) {
			/* Waiting */
			return;
		}

		int32_t err_us = DRIFT_MEAS_PERIOD_US - (sync_ctx->previous_sdu_ref_us -
							 sync_ctx->drift_comp.meas_start_time_us);

		int32_t freq_adj = APLL_FREQ_ADJ(err_us);

		sync_ctx->drift_comp.center_freq = APLL_FREQ_CENTER + freq_adj;

		if ((sync_ctx->drift_comp.center_freq > (APLL_FREQ_MAX)) ||
		    (sync_ctx->drift_comp.center_freq < (APLL_FREQ_MIN))) {
			LOG_DBG("Invalid center frequency, re-calculating");
			drift_comp_state_set(DRIFT_STATE_INIT);
			return;
		}

		hfclkaudio_set(sync_ctx->drift_comp.center_freq);

		drift_comp_state_set(DRIFT_STATE_OFFSET);
		break;
	}
	case DRIFT_STATE_OFFSET: {
		if (++sync_ctx->drift_comp.ctr < DRIFT_COMP_WAITING_CNT) {
			/* Waiting */
			return;
		}

		int32_t err_us = (sync_ctx->previous_sdu_ref_us - frame_start_ts) % BLK_PERIOD_US;

		if (err_us > (BLK_PERIOD_US / 2)) {
			err_us = err_us - BLK_PERIOD_US;
		}

		int32_t freq_adj = APLL_FREQ_ADJ(err_us);

		hfclkaudio_set(sync_ctx->drift_comp.center_freq + freq_adj);

		if ((err_us < DRIFT_ERR_THRESH_LOCK) && (err_us > -DRIFT_ERR_THRESH_LOCK)) {
			drift_comp_state_set(DRIFT_STATE_LOCKED);
		}

		break;
	}
	case DRIFT_STATE_LOCKED: {
		if (++sync_ctx->drift_comp.ctr < DRIFT_COMP_WAITING_CNT) {
			/* Waiting */
			return;
		}

		int32_t err_us = (sync_ctx->previous_sdu_ref_us - frame_start_ts) % BLK_PERIOD_US;

		if (err_us > (BLK_PERIOD_US / 2)) {
			err_us = err_us - BLK_PERIOD_US;
		}

		/* Use asymptotic correction with small errors */
		err_us /= 2;
		int32_t freq_adj = APLL_FREQ_ADJ(err_us);

		hfclkaudio_set(sync_ctx->drift_comp.center_freq + freq_adj);

		if ((err_us > DRIFT_ERR_THRESH_UNLOCK) || (err_us < -DRIFT_ERR_THRESH_UNLOCK)) {
			drift_comp_state_set(DRIFT_STATE_INIT);
		} else {
			sync_ctx->drift_comp.ctr = 0;
		}

		break;
	}
	default: {
		break;
	}
	}
}

static void pres_comp_state_set(struct sync *sync_ctx, enum pres_comp_state new_state)
{
	int ret;

	if (new_state == sync_ctx->pres_comp.state) {
		return;
	}

	sync_ctx->pres_comp.ctr = 0;

	sync_ctx->pres_comp.state = new_state;
	LOG_INF("Pres comp state: %s", pres_comp_state_names[new_state]);
	if (new_state == PRES_STATE_LOCKED) {
		ret = led_on(LED_APP_2_GREEN);
	} else {
		ret = led_off(LED_APP_2_GREEN);
	}
	ERR_CHK(ret);
}

/**
 * @brief Move audio blocks back and forth in FIFO to get audio in sync
 *
 * @note The audio sync is based on sdu_ref_us
 *
 */
void audio_sync_presentation_compensation(struct sync *sync_ctx, uint32_t recv_frame_ts_us,
					  uint32_t sdu_ref_us, bool sdu_ref_not_consecutive)
{
	if (sync_ctx->drift_comp.state != DRIFT_STATE_LOCKED) {
		/* Unconditionally reset state machine if drift compensation looses lock */
		pres_comp_state_set(PRES_STATE_INIT);
		return;
	}

	/* Move presentation compensation into PRES_STATE_WAIT if sdu_ref_us and
	 * previous sdu_ref_us origins from non-consecutive frames
	 */
	if (sdu_ref_not_consecutive) {
		pres_comp_state_set(PRES_STATE_WAIT);
	}

	int32_t wanted_pres_dly_us =
		sync_ctx->pres_comp.pres_delay_us - (recv_frame_ts_us - sdu_ref_us);
	int32_t pres_adj_us = 0;

	switch (sync_ctx->pres_comp.state) {
	case PRES_STATE_INIT: {
		sync_ctx->pres_comp.sum_err_dly_us = 0;
		pres_comp_state_set(PRES_STATE_MEAS);
		break;
	}
	case PRES_STATE_MEAS: {
		if (sync_ctx->pres_comp.ctr++ < PRES_COMP_NUM_DATA_PTS) {
			sync_ctx->pres_comp.sum_err_dly_us +=
				wanted_pres_dly_us - sync_ctx->current_pres_dly_us;

			/* Same state - Collect more data */
			break;
		}

		pres_adj_us = sync_ctx->pres_comp.sum_err_dly_us / PRES_COMP_NUM_DATA_PTS;
		if ((pres_adj_us >= (BLK_PERIOD_US / 2)) || (pres_adj_us <= -(BLK_PERIOD_US / 2))) {
			pres_comp_state_set(PRES_STATE_WAIT);
		} else {
			/* Drift compensation will always be in DRIFT_STATE_LOCKED here */
			pres_comp_state_set(PRES_STATE_LOCKED);
		}

		break;
	}
	case PRES_STATE_WAIT: {
		if (sync_ctx->pres_comp.ctr++ >
		    (FIFO_SMPL_PERIOD_US / CONFIG_AUDIO_FRAME_DURATION_US)) {
			pres_comp_state_set(PRES_STATE_INIT);
		}

		break;
	}
	case PRES_STATE_LOCKED: {
		/*
		 * Presentation delay compensation moves into PRES_STATE_WAIT if sdu_ref_us
		 * and previous sdu_ref_us origins from non-consecutive frames, or into
		 * PRES_STATE_INIT if drift compensation unlocks.
		 */

		break;
	}
	default: {
		break;
	}
	}

	if (pres_adj_us == 0) {
		return;
	}

	if (pres_adj_us >= 0) {
		pres_adj_us += (BLK_PERIOD_US / 2);
	} else {
		pres_adj_us += -(BLK_PERIOD_US / 2);
	}

	/* Number of adjustment blocks is 0 as long as |pres_adj_us| < BLK_PERIOD_US */
	int32_t pres_adj_blks = pres_adj_us / BLK_PERIOD_US;

	if (pres_adj_blks > (FIFO_NUM_BLKS / 2)) {
		/* Limit adjustment */
		pres_adj_blks = FIFO_NUM_BLKS / 2;

		LOG_WRN("Requested presentation delay out of range: pres_adj_us=%d", pres_adj_us);
	} else if (pres_adj_blks < -(FIFO_NUM_BLKS / 2)) {
		/* Limit adjustment */
		pres_adj_blks = -(FIFO_NUM_BLKS / 2);

		LOG_WRN("Requested presentation delay out of range: pres_adj_us=%d", pres_adj_us);
	}

	if (pres_adj_blks > 0) {
		LOG_DBG("Presentation delay inserted: pres_adj_blks=%d", pres_adj_blks);

		/* Increase presentation delay */
		for (int i = 0; i < pres_adj_blks; i++) {
			/* Mute audio block */
			memset(&sync_ctx->out
					.fifo[sync_ctx->out.prod_blk_idx * BLK_STEREO_NUM_SAMPS],
			       0, BLK_STEREO_SIZE_OCTETS);

			/* Record producer block start reference */
			sync_ctx->out.prod_blk_ts[sync_ctx->out.prod_blk_idx] =
				recv_frame_ts_us - ((pres_adj_blks - i) * BLK_PERIOD_US);

			sync_ctx->out.prod_blk_idx = NEXT_IDX(sync_ctx->out.prod_blk_idx);
		}
	} else if (pres_adj_blks < 0) {
		LOG_DBG("Presentation delay removed: pres_adj_blks=%d", pres_adj_blks);

		/* Reduce presentation delay */
		for (int i = 0; i > pres_adj_blks; i--) {
			sync_ctx->out.prod_blk_idx = PREV_IDX(sync_ctx->out.prod_blk_idx);
		}
	}
}
