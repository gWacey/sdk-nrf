/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>
#include <data_fifo.h>

#include "fakes.h"

/*
 * Stubs are defined here, so that multiple *.c files can share them
 * without having linker issues.
 */
DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(int, tone_gen_size, void*, size_t *, uint16_t, uint32_t, uint8_t, uint8_t, float);

/* Custom fakes implementation */
int fake_tone_gen_size__succeeds(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);
	uint8_t *tone_8_bit = (uint8_t *)tone;
	uint16_t *tone_16_bit = (uint16_t *)tone;
	uint32_t *tone_32_bit = (uint32_t *)tone;
	int32_t tone_samp;
	int32_t tone_step;
	uint16_t tone_samples = (FAKE_TONE_LENGTH_BYTES / (carrier / 8)) / 4;

	if (sample_bits / 8 == 1) {
		tone_step =  INT8_MAX / tone_samples;
	} else if (sample_bits / 8 == 2) {
		tone_step =  INT16_MAX / tone_samples;
	} else if (sample_bits / 8 == 3) {
		tone_step =  0x7FFFFF / tone_samples;
	} else {
		tone_step =  INT32_MAX / tone_samples;
	}

	for (size_t i = 0; i < tone_samples; i++) {
		tone_samp = i * tone_step;

		if (carrier / 8 == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier / 8 == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i < tone_samples; i++) {
		tone_samp = (tone_step * tone_samples) - (i * tone_step);

		if (carrier / 8 == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier / 8 == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i <tone_samples; i++) {
		tone_samp = -i * tone_step;

		if (carrier / 8 == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier / 8 == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	for (size_t i = 0; i <tone_samples; i++) {
		tone_samp = (i * tone_step) - (tone_step * tone_samples);

		if (carrier / 8 == 1) {
			*tone_8_bit++ = (uint8_t)tone_samp;
		} else if (carrier / 8 == 2) {
			*tone_16_bit++ = (uint16_t)tone_samp;
		} else {
			*tone_32_bit++ = (uint32_t)tone_samp;
		}
	}

	*tone_size = FAKE_TONE_LENGTH_BYTES;

	return 0;
}

int fake_tone_gen_size__null_size(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -ENXIO;
}

int fake_tone_gen_size__frequency_invalid(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -EINVAL;
}

int fake_tone_gen_size__amplitue_invalid_fails(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -EPERM;
}

int fake_tone_gen_size__bit_sizes_invalid_fails(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -EPERM;
}
