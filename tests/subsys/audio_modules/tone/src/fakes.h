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

/* Length of the fake tone. */
#define FAKE_TONE_LENGTH_BYTES (200)

/* Fake functions declaration. */
DECLARE_FAKE_VALUE_FUNC(int, tone_gen_size, void*, size_t *, uint16_t, uint32_t, uint8_t, uint8_t, float);

/* List of fakes used by this unit tester */
#define DO_FOREACH_FAKE(FUNC)                                                                      \
	do {                                                                                       \
		FUNC(tone_gen_size)                                            \
	} while (0)

int fake_tone_gen_size__succeeds(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude);
int fake_tone_gen_size__null_size(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude);
int fake_tone_gen_size__frequency_invalid(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude);
int fake_tone_gen_size__amplitue_invalid_fails(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude);
int fake_tone_gen_size__bit_sizes_invalid_fails(void *tone, size_t *tone_size, uint16_t tone_freq_hz, 
	     uint32_t smpl_freq_hz, uint8_t sample_bits, uint8_t carrier, float amplitude);

#endif /* _FAKES_H_ */
