/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _FAKES_H_
#define _FAKES_H_

#include <zephyr/ztest.h>
#include <zephyr/fff.h>
#include <tone.h>

/* Fake functions declaration. */
DECLARE_FAKE_VALUE_FUNC(int, tone_gen, int16_t *, size_t *, uint16_t, uint32_t, float);

/* List of fakes used by this unit tester */
#define DO_FOREACH_FAKE(FUNC)                                                                      \
	do {                                                                                       \
		FUNC(tone_gen)                                            \
	} while (0)

int fake_tone_gen__succeeds(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude);
int fake_tone_gen__null_size(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude);
int fake_tone_gen__frequency_invalid(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude);
int fake_tone_gen__amplitue_invalid_fails(int16_t *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, float amplitude);

#endif /* _FAKES_H_ */
