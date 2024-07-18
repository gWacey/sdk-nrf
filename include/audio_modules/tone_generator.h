/*
 * Copyright(c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _AUDIO_MODULE_TONE_GENERATOR_H_
#define _AUDIO_MODULE_TONE_GENERATOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "audio_defines.h"
#include "audio_module.h"

/**
 * @brief Private pointer to the module's parameters.
 *
 */
extern struct audio_module_description *audio_module_tone_gen_description;

/**
 * @brief The module configuration structure.
 *
 */
struct audio_module_tone_gen_configuration {
	/* Sample rate for the instance */
	uint32_t sample_rate_hz;

	/* Number of valid bits for a sample (bit depth).
	 * Typically 16 or 24.
	 */
	uint8_t bits_per_sample;

	/* Number of bits used to carry a sample of size bits_per_sample.
	 * For example, say we have a 24 bit sample stored in a 32 bit
	 * word (int32_t), then:
	 *     bits_per_sample = 24
	 *     carrier_size    = 32
	 */
	uint32_t carried_bits_per_sample;

	/* The amplitude of the tone to generate. */
	float amplitude;

	/* A flag indicating if the buffer is sample interleaved or not */
	bool interleaved;
};

/**
 * @brief Private module context.
 *
 */
struct audio_module_tone_gen_context {
	/* The tone generator configuration. */
	struct audio_module_tone_gen_configuration config;
};

#endif /* _AUDIO_MODULE_TONE_GENERATOR_H_ */
