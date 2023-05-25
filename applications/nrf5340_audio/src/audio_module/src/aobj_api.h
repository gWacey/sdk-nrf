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

enum aobj_type {
	/* The audio data object is raw samples */
	AOBJ_TYPE_SAMPLES = 0,

	/* The audio data object is coded data */
	AOBJ_TYPE_CODED = 1,

	/* The audio data object is user defined */
	AOBJ_TYPE_USER = 1
};

enum aobj_pcm_type {
	/* The audio data object contains samples in linear format */
	AOBJ_PCM_TYPE_LINEAR = 0,

	/* The audio data object contains samples in ulaw format */
	AOBJ_PCM_TYPE_ULAW = 1,

	/* The audio data object contains samples in Alaw format */
	AOBJ_PCM_TYPE_ALAW = 2
};

enum aobj_coding_type {
	/* The audio data object is coded in LC3 */
	AOBJ_CODING_TYPE_LC3 = 0,

	/* The audio data object is coded in LC3plus */
	AOBJ_CODING_TYPE_LC3PLUS = 1,
};

enum aobj_alignment {
	/* A sample is aligned to the left in the carrier */
	AOBJ_ALIGNMENT_LEFT = 0,

	/* A sample is aligned to the right in the carrier */
	AOBJ_ALIGNMENT_RIGHT = 1
};

enum aobj_sign {
	/* A sample is signed data */
	AOBJ_SIGNED = 0,

	/* A sample is unsigned data */
	AOBJ_UNSIGNED = 1
};

enum aobj_endianism {
	/* Audio samples are stored in little endian */
	AOBJ_ENDIAN_LITTLE = 0,

	/* Audio samples are stored in big endian */
	AOBJ_ENDIAN_BIG = 1
};

enum aobj_interleaved {
	/* Audio samples are deinterleaved within the buffer */
	AOBJ_DEINTERLEAVED = 0,

	/* Audio samples are interleaved within the buffer */
	AOBJ_INTERLEAVED = 1
};

/**
 * @brief  A structure describing the PCM data format
 *
 */
struct aobj_format {
	union type {
		/* An enum/defines list giving the PCM types supported (e.g., linear, Alaw, etc.) */
		enum aobj_pcm_type pcm_type;

		/* An enum/defines list giving the coding types supported (e.g., LC3, MP3, etc.) */
		enum aobj_coding_type coding_type;
	};

	/* The PCM sample rate */
	uint32_t sample_rate;

	/* Number of valid bits for a sample */
	uint8_t bits_per_sample;

	/* Number of bits used to carry a sample of size bits_per_sample*/
	uint8_t carrier_size;

	/* Flag indicating the alignment of the sample within the carrier */
	enum aobj_alignment alignment;

	/* Flag indicated if the sample is signed or unsigned */
	enum aobj_sign sign;

	/* Flag indicating the endianness of the sample */
	enum aobj_endianism sample_endianism;

	/* A flag indicating if the PCM object is sample interleaved or not */
	enum aobj_interleaved interleaved;

	/* Number of channels in the PCM object */
	uint8_t number_channels;

	/* A 32 bit array indicating which channel(s) are contained within
	 * the PCM object (1 = channel location, 0 = not this channel)
	 */
	uint32_t channel_map;
};

/**
 * @brief  A structure giving the description of the synchronisation data object
 *
 */
struct aobj_sync {
	/* The previous SDU reference position in time */
	uint32_t previous_sdu_ref_us;

	/* The current SDU reference position in time */
	uint32_t current_pres_dly_us;
};

/**
 * @brief  A structure giving the description of the audio object
 *
 */
struct aobj_object {
	/* Indicates the data type of the object coded or samples or user defined */
	enum aobj_type data_type;

	/* A pointer to the raw or coded data (e.g., PCM, LC3, etc.) */
	uint8_t *data;

	/* The size, in bytes of the raw data object */
	size_t data_size;

	/* A structure defining the audio data objects format */
	struct aobj_format format;

	/* A structure holding the data required for synchronised
	 * audio  (e.g., PTS, buff occupancy, etc.)
	 */
	struct aobj_sync sync_data;

	/* A Boolean flag to indicate this frame has errors in the
	 * data (::true = bad, fales:: = good)
	 */
	bool bad_frame;

	/* A Boolean flag to indicate the last buffer in the
	 * stream (fales:: = more to come, ::true = last buffer)
	 */
	bool last_flag;

	/* A pointer to private data associated with the data object */
	void *user_data;
};

#endif /* _AOBJ_API_H_ */
