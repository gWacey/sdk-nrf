/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _AOBJ_API_H_
#define _AOBJ_API_H_

#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Value to define the data type carried by the block.
 */
enum aobj_type {
	/* The audio data block type is undefined */
	AOBJ_TYPE_UNDEFINED = 0,

	/* The audio data block is raw PCM */
	AOBJ_TYPE_PCM,

	/* The audio data block is coded in LC3 */
	AOBJ_TYPE_LC3,

	/* The audio data block is coded in LC3plus */
	AOBJ_TYPE_LC3PLUS,
};

/**
 * @brief Value to define the PCM interleaving type.
 */
enum aobj_interleaved {
	/* The interleving is undefined */
	AOBJ_INTERLEAVE_UNDEFINED = 0,

	/* Audio samples are deinterleaved within the buffer */
	AOBJ_DEINTERLEAVED,

	/* Audio samples are interleaved within the buffer */
	AOBJ_INTERLEAVED
};

/**
 * @brief  A structure describing the PCM data format
 */
struct aobj_format {
	/* The PCM sample rate */
	uint32_t sample_rate;

	/* Number of valid bits for a sample */
	uint8_t bits_per_sample;

	/* Number of bits used to carry a sample of size bits_per_sample*/
	uint8_t carrier_size;

	/* A flag indicating if the PCM block is sample interleaved or not */
	enum aobj_interleaved interleaved;

	/* Number of channels in the PCM block */
	uint8_t number_channels;

	/* A 32 bit array indicating which channel(s) are contained within
	 * the PCM block (1 = channel location, 0 = not this channel)
	 */
	uint32_t channel_map;
};

/**
 * @brief  A structure giving the description of the audio block
 */
struct aobj_block {
	/* Indicates the data type of the block */
	enum aobj_type data_type;

	/* A pointer to the raw or coded data (e.g., PCM, LC3, etc.) */
	uint8_t *data;

	/* The size, in bytes of the raw data block */
	size_t data_size;

	/* A structure defining the audio data blocks format */
	struct aobj_format format;

	/* Bit rate of the block */
	uint8_t bitrate;

	/* Reference time stamp (e.g. ISO timestamp
	 * reference from BLE controller)
	 */
	uint32_t reference_ts;

	/* The timestamp for when the block was received */
	uint32_t block_rx_ts;

	/* A Boolean flag to indicate this frame has errors in the
	 * data (::true = bad, fales:: = good)
	 */
	bool bad_frame;

	/* A Boolean flag to indicate the last buffer in the
	 * stream (fales:: = more to come, ::true = last buffer)
	 */
	bool last_flag;

	/* A pointer to private data associated with the data block */
	void *user_data;
};

#endif /* _AOBJ_API_H_ */
