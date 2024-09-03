/*
 * Copyright(c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_ENCODER_H_
#define _LC3_ENCODER_H_

#include "audio_defines.h"
#include "audio_module.h"

#define LC3_ENCODER_PCM_NUM_BYTES_MONO                                                             \
	((CONFIG_LC3_ENCODER_SAMPLE_RATE_HZ * CONFIG_LC3_ENCODER_BIT_DEPTH_OCTETS *                \
	  CONFIG_LC3_ENCODER_FRAME_DURATION_US) /                                                  \
	 1000000)

/**
 * @brief Private pointer to the module's parameters.
 */
extern struct audio_module_description *lc3_encoder_description;

/**
 * @brief Private pointer to the encoders handle.
 */
struct lc3_encoder_handle;

/**
 * @brief The module configuration structure.
 */
struct lc3_encoder_configuration {
	/* Sample rate for the encoder instance. */
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

	/* Frame duration for this encoder instance. */
	uint32_t data_len_us;

	/* A flag indicating if the input buffer is sample interleaved or not. */
	bool interleaved;

	/* Channel locations for this encoder instance. */
	uint32_t locations;

	/* Maximum bitrate supported by the encoder. */
	uint32_t bitrate_bps_max;
};

/**
 * @brief  Private module context.
 */
struct lc3_encoder_context {
	/* Array of encoder channel handles. */
	struct lc3_encoder_handle *lc3_enc_channel[CONFIG_LC3_ENCODER_CHANNELS_MAX];

	/* Number of encoder channel handles. */
	uint32_t enc_handles_count;

	/* The encoder configuration. */
	struct lc3_encoder_configuration config;

	/* Minimum sample bytes per frame required for this encoder instance. */
	uint16_t sample_frame_req;

	/* Audio coded bytes per frame. */
	size_t coded_frame_bytes;
};

#endif /* _LC3_ENCODER_H_ */
