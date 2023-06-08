/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _AUDIO_SYNC_H_
#define _AUDIO_SYNC_H_

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

enum drift_comp_state {
	DRIFT_STATE_INIT, /* Waiting for data to be received */
	DRIFT_STATE_CALIB, /* Calibrate and zero out local delay */
	DRIFT_STATE_OFFSET, /* Adjust I2S offset relative to SDU Reference */
	DRIFT_STATE_LOCKED /* Drift compensation locked - Minor corrections */
};

enum pres_comp_state {
	PRES_STATE_INIT, /* Initialize presentation compensation */
	PRES_STATE_MEAS, /* Measure presentation delay */
	PRES_STATE_WAIT, /* Wait for some time */
	PRES_STATE_LOCKED /* Presentation compensation locked */
};

/**
 * @brief The sychronisations private context
 *
 */
struct sync {
	uint32_t previous_sdu_ref_us;
	uint32_t current_pres_dly_us;

	struct {
		enum drift_comp_state state;
		uint16_t ctr; /* Count func calls. Used for waiting */
		uint32_t meas_start_time_us;
		uint32_t center_freq;
		bool enabled;
	} drift_comp;

	struct {
		enum pres_comp_state state;
		uint16_t ctr; /* Count func calls. Used for collecting data points and waiting */
		int32_t sum_err_dly_us;
		uint32_t pres_delay_us;
		bool enabled;
	} pres_comp;
};

/**
 * @brief Set the presentation delay
 *
 * @param delay_us The presentation delay in µs
 *
 * @return 0 if successful, error otherwise
 */
int audio_sync_pres_delay_us_set(uint32_t delay_us);

/**
 * @brief Get the current presentation delay
 *
 * @param delay_us  The presentation delay in µs
 */
void audio_sync_pres_delay_us_get(uint32_t *delay_us);

/**
 * @brief Update sdu_ref_us so that drift compensation can work correctly
 *
 * @note This function is only valid for gateway using I2S as audio source
 *       and unidirectional audio stream (gateway to headset(s))
 *
 * @param sdu_ref_us    ISO timestamp reference from BLE controller
 * @param adjust        Indicate if the sdu_ref should be used to adjust timing
 */
void audio_sync_sdu_ref_update(uint32_t sdu_ref_us, bool adjust);

/**
 * @brief Adjust frequency of HFCLKAUDIO to get audio in sync
 *
 * @note The audio sync is based on sdu_ref_us
 *
 * @param sync_ctx        Pointer to the synchronisation context
 * @param frame_start_ts  I2S frame start timestamp
 */
void audio_sync_drift_compensation(struct sync *sync_ctx, uint32_t frame_start_ts);

/**
 * @brief Move audio blocks back and forth in FIFO to get audio in sync
 *
 * @note The audio sync is based on sdu_ref_us
 *
 * @param sync_ctx                 Pointer to the synchronisation context
 * @param recv_frame_ts_us         Timestamp of when frame was received
 * @param sdu_ref_us               ISO timestamp reference from BLE controller
 * @param sdu_ref_not_consecutive  True if sdu_ref_us and previous sdu_ref_us
 *				                   origins from non-consecutive frames
 */
void audio_sync_presentation_compensation(struct sync *sync_ctx, uint32_t recv_frame_ts_us,
					  uint32_t sdu_ref_us, bool sdu_ref_not_consecutive);

#endif /* _AUDIO_SYNC_H_ */
