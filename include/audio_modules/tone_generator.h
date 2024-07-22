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

/* @brief Calculate the maximum size of the tone buffer in 16 bit samples. */
#define TONE_GEN_BUFFER_SIZE_MAX ((CONFIG_TONE_GENERATION_SAMPLE_RATE_HZ_MAX / CONFIG_TONE_GENERATION_FREQUENCY_HZ_MAX) * sizeof(uint8_t))

/**
 * @brief Tone generator mix options.
 */
enum tone_gen_mix_option {
	TONE_GEN_NO_MIX,
	TONE_GEN_MIX_ALL
};

/**
 * @brief Private pointer to the module's parameters.
 */
extern struct audio_module_description *audio_module_tone_gen_description;

/**
 * @brief The module configuration structure.
 */
struct audio_module_tone_gen_configuration {
	/* Frequency of the tone to generate, in the range [100..10000] Hz. */
	uint16_t frequency_hz;

	/* The amplitude of the tone to generate, in the range [0..1]. */
	float amplitude;

	/* Mixer control option. */
	enum tone_gen_mix_option mix_opt;
};

/**
 * @brief Private module context.
 */
struct audio_module_tone_gen_context {
	/* Single tone cycle sample buffer. */
	uint16_t tone_buffer[TONE_GEN_BUFFER_SIZE_MAX];

	/* Number of bytes for one tone cycle. */
	size_t cycle_bytes_num;

	/* Byte position of the next byte in the tone buffer. */
	size_t byte_remain_index;

	/* Number of bytes for a sample. */
	uint8_t bytes_per_sample;

	/* Number of bytes used to carry a sample of size bytes_per_sample.
	 * For example, say we have a 24 bit sample stored in a 32 bit
	 * word (int32_t), then:
	 *     bytes_per_sample = 3
	 *     carrier_size     = 4
	 */
	uint32_t carried_bytes_per_sample;

	/* Metadat defining the tone buffer. */
	struct audio_metadata meta;

	/* The tone generator configuration. */
	struct audio_module_tone_gen_configuration config;
};

#endif /* _AUDIO_MODULE_TONE_GENERATOR_H_ */
