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
 * @brief Value to define the data type carried by the object.
 *
 */
enum aobj_type {
	/*! The audio data object is raw PCM */
	AOBJ_TYPE_PCM = 0,

	/*! The audio data object is coded in LC3 */
	AOBJ_CODING_TYPE_LC3,

	/*! The audio data object is coded in LC3plus */
	AOBJ_CODING_TYPE_LC3PLUS,
};

/**
 * @brief Value to define the PCM interleaving type.
 *
 */
enum aobj_interleaved {
	/*! Audio samples are deinterleaved within the buffer */
	AOBJ_DEINTERLEAVED = 0,

	/*! Audio samples are interleaved within the buffer */
	AOBJ_INTERLEAVED = 1
};

/**
 * @brief  A structure describing the PCM data format
 *
 */
struct aobj_format {
	/*! The PCM sample rate */
	uint32_t sample_rate;

	/*! Number of valid bits for a sample */
	uint8_t bits_per_sample;

	/*! Number of bits used to carry a sample of size bits_per_sample*/
	uint8_t carrier_size;

	/*! A flag indicating if the PCM object is sample interleaved or not */
	enum aobj_interleaved interleaved;

	/*! Number of channels in the PCM object */
	uint8_t number_channels;

	/*! A 32 bit array indicating which channel(s) are contained within
	 * the PCM object (1 = channel location, 0 = not this channel)
	 */
	uint32_t channel_map;
};

/**
 * @brief  A structure giving the description of the synchronisation data object
 *
 */
struct aobj_sync {
	/*! The previous SDU reference position in time */
	uint32_t previous_sdu_ref_us;

	/*! The current SDU reference position in time */
	uint32_t current_pres_dly_us;
};

/**
 * @brief  A structure giving the description of the audio object
 *
 */
struct aobj_object {
	/*! Indicates the data type of the object */
	enum aobj_type data_type;

	/*! A pointer to the raw or coded data (e.g., PCM, LC3, etc.) */
	uint8_t *data;

	/*! The size, in bytes of the raw data object */
	size_t data_size;

	/*! A structure defining the audio data objects format */
	struct aobj_format format;

	/*! A structure holding the data required for synchronised
	 * audio  (e.g., PTS, buff occupancy, etc.)
	 */
	struct aobj_sync sync_data;

	/*! A Boolean flag to indicate this frame has errors in the
	 * data (::true = bad, fales:: = good)
	 */
	bool bad_frame;

	/*! A Boolean flag to indicate the last buffer in the
	 * stream (fales:: = more to come, ::true = last buffer)
	 */
	bool last_flag;

	/*! A pointer to private data associated with the data object */
	void *user_data;
};

#endif /* _AOBJ_API_H_ */
