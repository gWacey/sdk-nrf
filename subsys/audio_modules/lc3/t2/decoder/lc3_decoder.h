/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_DECODER_H_
#define _LC3_DECODER_H_

#include "audio_defines.h"
#include "audio_module.h"
#include "LC3API.h"

#define LC3_ENC_MONO_FRAME_SIZE                                                                    \
	(CONFIG_SW_LC3_BITRATE_MAX * CONFIG_SW_LC3_FRAME_DURATION_US / (8 * 1000000))

#define LC3_PCM_NUM_BYTES_MONO                                                                     \
	(CONFIG_SW_LC3_SAMPLE_RATE_HZ * CONFIG_SW_LC3_BIT_DEPTH_OCTETS *                           \
	 CONFIG_SW_LC3_FRAME_DURATION_US / 1000000)
#define LC3_ENC_TIME_US 3000
#define LC3_DEC_TIME_US 1500

/* Max will be used when multiple codecs are supported */
#define LC3_DEC_CHANNELS_MAX	     CONFIG_LC3_DEC_CHAN_MAX
#define DEC_TIME_US		     MAX(LC3_DEC_TIME_US, 0)
#define DEC_PCM_NUM_BYTES_MONO	     MAX(LC3_PCM_NUM_BYTES_MONO, 0)
#define DEC_PCM_NUM_BYTES_MULTI_CHAN (DEC_PCM_NUM_BYTES_MONO * LC3_DEC_CHANNELS_MAX)

/**
 * @brief Private pointer to the module's parameters.
 */
extern struct audio_module_description *lc3_decoder_description;

/**
 * @brief Private pointer to the decoders handle.
 */
struct lc3_decoder_handle;

/**
 * @brief The module configuration structure.
 */
struct lc3_decoder_configuration {
	/* Sample rate for the decoder instance */
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
	uint32_t carried_bits_pr_sample;

	/* Frame duration for this decoder instance */
	uint32_t data_len_us;

	/* A flag indicating if the decoded buffer is sample interleaved or not */
	bool interleaved;

	/* Active channel locations for this decoder instance */
	uint32_t locations;

	/* Maximum bitrate supported by the decoder. */
	uint32_t bitrate_bps;
};

/**
 * @brief  Private module context.
 */
struct lc3_decoder_context {
	/* Array of decoder channel handles */
	struct lc3_decoder_handle *lc3_dec_channel[LC3_DEC_CHANNELS_MAX];

	/* Number of decoder channel handles */
	uint32_t dec_handles_count;

	/* The decoder configuration */
	struct lc3_decoder_configuration config;

	/* Minimum coded bytes required for this decoder instance */
	uint16_t coded_bytes_req;

	/* Audio samples per frame */
	size_t samples_per_frame;

	/* Number of successive frames to which PLC has been applied */
	uint16_t plc_count;
};

#endif /* _LC3_DECODER_H_ */
