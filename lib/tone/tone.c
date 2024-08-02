/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <tone.h>

#include <zephyr/kernel.h>
#include <math.h>
#include <stdio.h>
#include <arm_math.h>

#define FREQ_LIMIT_LOW 100
#define FREQ_LIMIT_HIGH 10000

int tone_gen(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, uint32_t smpl_freq_hz,
	     float amplitude)
{
	if (tone == NULL || tone_size == NULL) {
		return -ENXIO;
	}

	if (!smpl_freq_hz || tone_freq_hz < FREQ_LIMIT_LOW || tone_freq_hz > FREQ_LIMIT_HIGH) {
		return -EINVAL;
	}

	if (amplitude > 1 || amplitude <= 0) {
		return -EPERM;
	}

	uint32_t samples_for_one_period = smpl_freq_hz / tone_freq_hz;

	for (uint32_t i = 0; i < samples_for_one_period; i++) {
		float curr_val = i * 2 * PI / samples_for_one_period;
		float32_t res = arm_sin_f32(curr_val);
		/* Generate one sine wave */
		tone[i] = amplitude * res * INT16_MAX;
	}

	/* Configured for bit depth 16 */
	*tone_size = (size_t)samples_for_one_period * 2;

	return 0;
}


static int tone_gen_8(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint32_t max, float amplitude)
{
	uint32_t samples_for_one_period = smpl_freq_hz / tone_freq_hz;

	for (uint32_t i = 0; i < samples_for_one_period; i++) {
		float curr_val = i * 2 * PI / samples_for_one_period;
		float32_t res = arm_sin_f32(curr_val);
		/* Generate one sine wave */
		tone[i] = amplitude * res * max;
	}

	*tone_size = (size_t)samples_for_one_period;

	return 0;
}

static int tone_gen_32(int32_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, int32_t max, float amplitude)
{
	uint32_t samples_for_one_period = smpl_freq_hz / tone_freq_hz;

	for (uint32_t i = 0; i < samples_for_one_period; i++) {
		float curr_val = i * 2 * PI / samples_for_one_period;
		float32_t res = arm_sin_f32(curr_val);
		/* Generate one sine wave */
		tone[i] = amplitude * res * max;
	}

	*tone_size = (size_t)samples_for_one_period * 4;

	return 0;
}

int tone_gen_size(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	int32_t max;
	
	if (tone == NULL || tone_size == NULL) {
		return -ENXIO;
	}

	if (!smpl_freq_hz || tone_freq_hz < FREQ_LIMIT_LOW || tone_freq_hz > FREQ_LIMIT_HIGH) {
		return -EINVAL;
	}

	if (amplitude > 1 || amplitude <= 0) {
		return -EPERM;
	}

	if (!sample_bits || !carrier || sample_bits > carrier || sample_bits > 32 || carrier > 32) {
		return -EPERM;
	}

	if (sample_bits == 8) {
		max =  INT8_MAX;
	} else if (sample_bits == 16) {
		max =  INT16_MAX;
	} else if (sample_bits == 24) {
		max =  0x007FFFFF;
	} else  {
		max =  INT32_MAX;
	} else {
		return -EINVAL;
	}

	if (carrier == 8) {
		ret = tone_gen_8((int8_t *)tone, tone_size, tone_freq_hz, smpl_freq_hz, max, amplitude);
	} else if (carrier == 16) {
		ret = tone_gen((int16_t *)tone, tone_size, tone_freq_hz, smpl_freq_hz, amplitude);
	} else {
		ret = tone_ge_32((int32_t *)tone, tone_size, tone_freq_hz, smpl_freq_hz, max, amplitude);
	}

	return 0;
}
