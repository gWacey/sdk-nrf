/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __SAMPLE_RATE_CONV_AWARE__
#define __SAMPLE_RATE_CONV_AWARE__

#include <stdint.h>
#include <stddef.h>

#include "audio_defines.h"

int sample_rate_conv_aware_init(uint32_t input_sample_rate, uint32_t output_sample_rate,
				enum audio_channel channel);

int sample_rate_conv_aware(void *input, size_t input_size, uint32_t input_sample_rate,
			   void *output, size_t *output_size, uint32_t output_sample_rate,
			   uint8_t pcm_bit_depth, enum audio_channel channel);

#endif /* __SAMPLE_RATE_CONV_AWARE__ */
