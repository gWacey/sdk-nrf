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

DEFINE_FAKE_VALUE_FUNC(int, tone_gen, int16_t *, size_t *, uint16_t, uint32_t, float);

/* Custom fakes implementation */
int fake_tone_gen__succeeds(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	memset(tone, 0, 100);
	*tone_size = 100;

	return 0;
}

int fake_tone_gen__null_size(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -ENXIO;
}

int fake_tone_gen__frequency_invalid(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -EINVAL;
}

int fake_tone_gen__amplitue_invalid_fails(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude)
{
	ARG_UNUSED(tone);
	ARG_UNUSED(tone_size);
	ARG_UNUSED(tone_freq_hz);
	ARG_UNUSED(smpl_freq_hz);
	ARG_UNUSED(amplitude);

	return -EPERM;
}
