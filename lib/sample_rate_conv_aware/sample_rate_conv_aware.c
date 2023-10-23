/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sample_rate_conv_aware.h"
#include "sample_rate_conv_aware_filters.h"
#include "audio_defines.h"

#include <dsp/filtering_functions.h>

#include <stdbool.h>

#include <zephyr/timing/timing.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sample_rate_conv_aware, 4);

#define BLOCK_SIZE		 480
#define BLOCK_SIZE_INTERPOLATION	(BLOCK_SIZE / 2)
#define BLOCK_SIZE_DECIMATION	BLOCK_SIZE
#define SRC_FILTER_NUMBER_TAPS (AWARE_FILTER_48KHZ_24KHZ_BIG_TAP_NUM)
#define STATE_SIZE_INTERPOLATION ((SRC_FILTER_NUMBER_TAPS / 2) + (BLOCK_SIZE_INTERPOLATION - 1))
#define STATE_SIZE_DECIMATION	 (SRC_FILTER_NUMBER_TAPS + (BLOCK_SIZE_DECIMATION - 1))

// Use these to calculate minimum size
// #define STATE_SIZE_INTERPOLATE	 (FILTER_TAP_NUM / 3) + BLOCK_SIZE - 1
// #define STATE_SIZE_DECIMATE	 FILTER_TAP_NUM + BLOCK_SIZE - 1

enum conversion_direction {
	NONE,
	UP,
	DOWN
};

enum conversion_factor {
	ZERO,
	TWO,
	THREE
};

static struct sample_rate_conv_filter {
	bool initialized;

	union {
		arm_fir_decimate_instance_q15 fir_decimate;
		arm_fir_interpolate_instance_q15 fir_interpolate;
	} filter_context;

	union {
		q15_t state_buf_decimate[STATE_SIZE_DECIMATION];
		q15_t state_buf_interpolate[STATE_SIZE_INTERPOLATION];
	} filter_state;

	size_t number_of_taps;
	q15_t *filter;
	uint8_t conversion_ratio;
	enum conversion_direction conv_dir;
	enum conversion_factor conv_factor;
} sample_rate_ctx[AUDIO_CH_NUM];

int sample_rate_conv_aware_init(uint32_t input_sample_rate, uint32_t output_sample_rate,
	enum audio_channel channel)
{
	arm_status err;

	if (sample_rate_ctx[channel].initialized == true) {
		return 0;
	}

	sample_rate_ctx[channel].filter = aware_filter_48khz_24khz_big;
	sample_rate_ctx[channel].number_of_taps = SRC_FILTER_NUMBER_TAPS;

	if (input_sample_rate == output_sample_rate) {
		LOG_ERR("NONE");
		sample_rate_ctx[channel].conv_dir = NONE;
		sample_rate_ctx[channel].conversion_ratio = 1;
	} else if (input_sample_rate > output_sample_rate) {
		sample_rate_ctx[channel].conv_dir = DOWN;
		sample_rate_ctx[channel].conversion_ratio = input_sample_rate / output_sample_rate;
	} else {
		sample_rate_ctx[channel].conv_dir = UP;
		sample_rate_ctx[channel].conversion_ratio = output_sample_rate / input_sample_rate;
	}

	LOG_DBG("Channel %d conversion %s ratio: %d", channel,
	(sample_rate_ctx[channel].conv_dir == UP ? "UP" : "DOWN"),
	sample_rate_ctx[channel].conversion_ratio);

	switch (sample_rate_ctx[channel].conv_dir) {
	case UP:
		err = arm_fir_interpolate_init_q15(
			&sample_rate_ctx[channel].filter_context.fir_interpolate,
			sample_rate_ctx[channel].conversion_ratio,
			sample_rate_ctx[channel].number_of_taps,
			sample_rate_ctx[channel].filter,
			&sample_rate_ctx[channel].filter_state.state_buf_interpolate[0],
			BLOCK_SIZE_INTERPOLATION);
		if (err != ARM_MATH_SUCCESS) {
			LOG_ERR("Failed to initialize interpolator (%d)", err);
			return err;
		}
		break;

	case DOWN:
		err = arm_fir_decimate_init_q15(
			&sample_rate_ctx[channel].filter_context.fir_decimate,
			sample_rate_ctx[channel].number_of_taps,
			sample_rate_ctx[channel].conversion_ratio,
			sample_rate_ctx[channel].filter,
			&sample_rate_ctx[channel].filter_state.state_buf_decimate[0],
			BLOCK_SIZE_DECIMATION);
		if (err != ARM_MATH_SUCCESS) {
			LOG_ERR("Failed to initialize decimator (%d)", err);
			return err;
		}
		break;
	case NONE:
		break;
	}

	LOG_ERR("Resampler initialized");
	sample_rate_ctx[channel].initialized = true;

	return 0;
}

int sample_rate_conv_aware(void *input, size_t input_size, uint32_t input_sample_rate,
			   void *output, size_t *output_size, uint32_t output_sample_rate,
			   uint8_t pcm_bit_depth, enum audio_channel channel)
{
	static uint32_t counter;
	timing_t start_time, end_time;
	uint64_t total_cycles;
	uint64_t total_ns;
	int bytes_per_sample = pcm_bit_depth / 8;

	timing_init();
	timing_start();

	switch (sample_rate_ctx[channel].conv_dir) {
	case UP:
		// LOG_ERR("Upsampling (%d)", input_size);
		start_time = timing_counter_get();
		arm_fir_interpolate_q15(&sample_rate_ctx[channel].filter_context.fir_interpolate,
					(q15_t *)input, (q15_t *)output,
					input_size / bytes_per_sample);
		end_time = timing_counter_get();
		*output_size = input_size * sample_rate_ctx[channel].conversion_ratio;
		break;

	case DOWN:
		// LOG_ERR("Downsampling (%d)", input_size);
		start_time = timing_counter_get();
		arm_fir_decimate_q15(&sample_rate_ctx[channel].filter_context.fir_decimate,
				(q15_t *)input, (q15_t *)output,
				input_size / bytes_per_sample);
		end_time = timing_counter_get();
		*output_size = (input_size) / sample_rate_ctx[channel].conversion_ratio;
		break;
	case NONE:
		start_time = timing_counter_get();
		memcpy(output, input, input_size);
		end_time = timing_counter_get();
		*output_size = input_size;
		break;
	}

	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);
	timing_stop();

	if ((counter++ % 1000) == 0) {
		LOG_ERR("Time used: %lf ms (input size %d)\n", total_ns / 1000000.0, input_size);
	}

	return 0;
}
