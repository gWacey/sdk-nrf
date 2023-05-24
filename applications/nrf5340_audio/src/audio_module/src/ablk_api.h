/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _ABLK_API_H_
#define _ABLK_API_H_

#include <zephyr/kernel.h>

/**
 * @brief Value to define the data type carried by the block.
 */
enum ablk_type {
	/* The audio data block type is undefined */
	ABLK_TYPE_UNDEFINED = 0,

	/* The audio data block is raw PCM */
	ABLK_TYPE_PCM,

	/* The audio data block is coded in LC3 */
	ABLK_TYPE_LC3
};

/**
 * @brief Value to define the PCM interleaving type.
 */
enum ablk_interleaved {
	/* The interleaving is undefined */
	ABLK_INTERLEAVE_UNDEFINED = 0,

	/* Audio samples are deinterleaved within the buffer */
	ABLK_DEINTERLEAVED,

	/* Audio samples are interleaved within the buffer */
	ABLK_INTERLEAVED
};

/**
 * @brief A structure describing the PCM data format
 */
struct ablk_pcm_format {
	/* The PCM sample rate in Hz */
	uint32_t sample_rate_hz;

	/* Number of valid bits for a sample */
	uint8_t bits_per_sample;

	/* Number of bits used to carry a sample of size bits_per_sample.
	 * For example, say we have a 24 bit sample stored in a 32 bit
	 * word (int32_t), then:
	 *     bits_per_sample = 24
	 *     carrier_size    = 32
	 */
	uint8_t carrier_size;

	/* A flag indicating if the samples are interleaved or not */
	enum ablk_interleaved interleaved;

	/* A 32 bit mask indicating which channel(s) are active within
	 * the PCM block. A bit set indicates the channel is active and
	 * a count of these will give the number of channels within the
	 * audio stream.
	 * Note: This will follow the ANSI/CTA-861-G’s Table 34 codes
	 * for speaker placement (as used by Bluetooth Low Energy Audio).
	 */
	uint32_t channel_map;
};

/**
 * @brief A structure giving the description of the audio block
 */
struct ablk_block {
	/* Indicates the data type of the block */
	enum ablk_type data_type;

	/* A pointer to the PCM or coded data (e.g., PCM, LC3, etc.) buffer */
	uint8_t *data;

	/* The size in bytes of the data buffer */
	size_t data_size;

	/* A structure defining the audio data blocks
	 * format, only valid if data_type == ABLK_TYPE_PCM
	 */
	struct ablk_pcm_format format;

	/* Bit rate of the block */
	uint8_t bitrate;

	/* Reference time stamp (e.g. ISO timestamp
	 * reference from BLE controller)
	 */
	uint32_t reference_ts;

	/* The timestamp for when the data was received */
	uint32_t data_rx_ts;

	/* A Boolean flag to indicate this frame has errors in the
	 * data (true = bad, false = good).
	 * Note: Timestamps may still be valid for a bad frame.
	 */
	bool bad_frame;

	/* A pointer to private data associated with the data block */
	void *user_data;
};

#endif /* _ABLK_API_H_ */
