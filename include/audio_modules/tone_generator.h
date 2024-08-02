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
#define TONE_GEN_BUFFER_SIZE_MAX ((CONFIG_TONE_GENERATION_SAMPLE_RATE_HZ_MAX / CONFIG_TONE_GENERATION_FREQUENCY_HZ_MIN) * sizeof(int16_t))

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

	/* Flag to indicate if the output will be interleaved or 
	 * deinterleaved.
	 * If true the output will be interleaved and if false
	 * the output will be deinterleaved. 
	 */
	bool interleave_output;

	/* Channels to mix the tone into. */
	uint32_t mix_locations;

	/* Scale factor to apply to the tone prior 
	 * to mixing, in the range [0..1].
	 */
	float tone_scale;

	/* Scale factor to apply to the input PCM buffer 
	 * prior to mixing, in the range [0..1].
	 */
	float input_scale;
};

/**
 * @brief Private module context.
 */
struct audio_module_tone_gen_context {
	/* Single tone cycle sample buffer. */
	uint16_t tone_buffer[TONE_GEN_BUFFER_SIZE_MAX];

	/* Description of the tone */
	int32_t tone_finite;

	/* Audio data structre defining the tone buffer. */
	struct audio_data tone_audio_data;

	/* Tone scale factor. */
	uint32_t tone_int_scale;

	/* PCM input scale factor. */
	uint32_t pcm_int_scale;

	/* The tone generator configuration. */
	struct audio_module_tone_gen_configuration config;
	
	/* Sample position of the next sample in the tone buffer. */
	uint32_t finite_pos;
};

#endif /* _AUDIO_MODULE_TONE_GENERATOR_H_ */
