/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sample_rate_conv_aware.h"
#include "sample_rate_conv_aware_filters.h"

#include <dsp/filtering_functions.h>

#include <stdbool.h>

#include <zephyr/timing/timing.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sample_rate_conv_aware, 3);

#define BLOCK_SIZE_INTERPOLATION 600
#define BLOCK_SIZE_DECIMATION	 700
#define BLOCK_SIZE		 480

// Use these to calculate minimum size
// #define STATE_SIZE_INTERPOLATE	 (FILTER_TAP_NUM / 3) + BLOCK_SIZE - 1
// #define STATE_SIZE_DECIMATE	 FILTER_TAP_NUM + BLOCK_SIZE - 1

static arm_fir_interpolate_instance_q15 fir_interpolate;
static arm_fir_decimate_instance_q15 fir_decimate;

static bool initialized;

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

q15_t state_buf_interpolate[BLOCK_SIZE_INTERPOLATION];
q15_t state_buf_decimate[BLOCK_SIZE_DECIMATION];

static uint32_t conversion_ratio;

static struct sample_rate_conv_filter {
	size_t number_of_taps;
	q15_t *filter;
	int conversion_ratio;
	enum conversion_direction conv_dir;
	enum conversion_factor conv_factor;
} sample_rate_ctx;

int sample_rate_conv_aware_init(uint32_t input_sample_rate, uint32_t output_sample_rate)
{
	arm_status err;

	if (initialized == true) {
		return 0;
	}

	sample_rate_ctx.filter = aware_filter_48khz_16khz_small;
	sample_rate_ctx.number_of_taps = AWARE_FILTER_48KHZ_16KHZ_BIG_TAP_NUM;

	if (input_sample_rate == output_sample_rate) {
		LOG_ERR("NONE");
		sample_rate_ctx.conv_dir = NONE;
	} else if (input_sample_rate > output_sample_rate) {
		sample_rate_ctx.conv_dir = DOWN;
		conversion_ratio = input_sample_rate / output_sample_rate;
	} else {
		sample_rate_ctx.conv_dir = UP;
		conversion_ratio = output_sample_rate / input_sample_rate;
	}

	LOG_ERR("Conversion ratio: %d", conversion_ratio);

	switch (sample_rate_ctx.conv_dir) {
	case UP:
		err = arm_fir_interpolate_init_q15(
			&fir_interpolate, conversion_ratio, sample_rate_ctx.number_of_taps,
			sample_rate_ctx.filter, state_buf_interpolate, BLOCK_SIZE);
		if (err != ARM_MATH_SUCCESS) {
			LOG_ERR("Failed to initialize interpolator (%d)", err);
			return err;
		}
		break;

	case DOWN:
		err = arm_fir_decimate_init_q15(&fir_decimate, sample_rate_ctx.number_of_taps,
						conversion_ratio, sample_rate_ctx.filter,
						state_buf_decimate, BLOCK_SIZE);
		if (err != ARM_MATH_SUCCESS) {
			LOG_ERR("Failed to initialize decimator (%d)", err);
			return err;
		}
		break;
	case NONE:
		break;
	}
	LOG_ERR("Resampler initialized");
	initialized = true;

	return 0;
}

int sample_rate_conv_aware(void *input, size_t input_size, uint32_t input_sample_rate, void *output,
			   size_t *output_size, uint32_t output_sample_rate, uint8_t pcm_bit_depth)
{
	static uint32_t counter;
	timing_t start_time, end_time;
	uint64_t total_cycles;
	uint64_t total_ns;

	timing_init();
	timing_start();

	switch (sample_rate_ctx.conv_dir) {
	case UP:
		// LOG_ERR("Upsampling (%d)", input_size);
		start_time = timing_counter_get();
		arm_fir_interpolate_q15(&fir_interpolate, (q15_t *)input, (q15_t *)output,
					input_size / 2);
		end_time = timing_counter_get();
		*output_size = input_size * conversion_ratio;
		break;

	case DOWN:
		// LOG_ERR("Downsampling (%d)", input_size);
		start_time = timing_counter_get();
		arm_fir_decimate_q15(&fir_decimate, (q15_t *)input, (q15_t *)output,
				     input_size / 2);
		end_time = timing_counter_get();
		*output_size = (input_size) / conversion_ratio;
		break;
	case NONE:
		start_time = timing_counter_get();
		memcpy(output, input, input_size);
		end_time = timing_counter_get();
		*output_size = input_size;
		break;
	}

	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);
	timing_stop();

	if ((counter++ % 1000) == 0) {
		LOG_ERR("Time used: %lf ms (input size %d)\n", total_ns / 1000000.0, input_size);
	}

	return 0;
}
