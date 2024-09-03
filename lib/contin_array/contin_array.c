/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <contin_array.h>

#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include "audio_defines.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(contin_array, CONFIG_CONTIN_ARRAY_LOG_LEVEL);

int contin_array_create(void *const pcm_cont, uint32_t pcm_cont_size, void const *const pcm_finite,
			uint32_t pcm_finite_size, uint32_t *const finite_pos)
{
	LOG_DBG("pcm_cont_size: %d pcm_finite_size %d", pcm_cont_size, pcm_finite_size);

	if (pcm_cont == NULL || pcm_finite == NULL) {
		return -ENXIO;
	}

	if (!pcm_cont_size || !pcm_finite_size) {
		LOG_ERR("size cannot be zero");
		return -EPERM;
	}

	for (uint32_t i = 0; i < pcm_cont_size; i++) {
		if (*finite_pos > (pcm_finite_size - 1)) {
			*finite_pos = 0;
		}
		((char *)pcm_cont)[i] = ((char *)pcm_finite)[*finite_pos];
		(*finite_pos)++;
	}

	return 0;
}

int cont_array_chans_create(struct audio_data *pcm_cont, struct audio_data *pcm_finite,
		  uint8_t channels, bool interleaved, uint32_t *const finite_pos)
{
	uint32_t step;
	uint8_t *output;
	uint8_t carrier_bytes = pcm_cont->meta.carried_bits_per_sample / 8;
	uint32_t tone_pos = *finite_pos;
	uint32_t frame_step;
	uint32_t bytes_per_channel;
	uint32_t out_chan;
	uint8_t chan;

	if (pcm_cont->data == NULL || pcm_finite->data == NULL) {
		LOG_ERR("data pointers can not be NULL");
		return -ENXIO;
	}

	if (!pcm_cont->data_size || !pcm_finite->data_size) {
		LOG_ERR("size cannot be zero");
		return -EPERM;
	}

	if (pcm_cont->meta.bits_per_sample != pcm_finite->meta.bits_per_sample ||
	 pcm_cont->meta.carried_bits_per_sample != pcm_finite->meta.carried_bits_per_sample) {
		LOG_ERR("sample/carrier size miss match");
		return -EINVAL;
	}

	if (channels == 0) {
		LOG_ERR("number of channels cannot be zero");
		return -EPERM;
	}

	bytes_per_channel = pcm_cont->data_size / channels;

	if (interleaved) {
		step = carrier_bytes * (channels - 1);
		frame_step = step;
	} else {
		step = 0;
		frame_step = bytes_per_channel * (channels - 1);
	}

	if (pcm_cont->meta.locations & pcm_finite->meta.locations) {
		out_chan = pcm_finite->meta.locations;
	} else {
		LOG_ERR("pcm_con does not have a channel(s) to extend into: %d   %d", pcm_cont->meta.locations, pcm_finite->meta.locations);
		return -EINVAL;
	}

	chan = 0;

	do {
		if (out_chan & 0x01) {
			output = &((uint8_t *)pcm_cont->data)[frame_step * chan];
			tone_pos = *finite_pos;

			for (uint32_t i = 0; i < bytes_per_channel; i += carrier_bytes) {
				for (uint32_t j = 0; j < carrier_bytes; j++) {
					if (tone_pos > (pcm_finite->data_size - 1)) {
						tone_pos = 0;
					}

					*output++ = ((uint8_t *)pcm_finite->data)[tone_pos];
					tone_pos++;
				}

				output += step;
			}

			chan += 1;
			channels -= 1;
		} 
		
		out_chan >>= 1;
	} while (channels);

	*finite_pos = tone_pos;

	return 0;
}
