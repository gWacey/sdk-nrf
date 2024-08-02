/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <pcm_mix.h>

#include <zephyr/kernel.h>
#include "audio_defines.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pcm_mix, CONFIG_PCM_MIX_LOG_LEVEL);

/* Clip signal if amplitude is outside legal range */
static void hard_limiter(int32_t *const pcm)
{
	if (*pcm < INT16_MIN) {
		LOG_DBG("Clip");
		*pcm = INT16_MIN;
	} else if (*pcm > INT16_MAX) {
		LOG_DBG("Clip");
		*pcm = INT16_MAX;
	}
}

/* Mix stereo-stereo or mono-mono. I.e. buffers are of equal size */
static void pcm_mix_identical(void *const pcm_a, size_t size_a, void const *const pcm_b,
			      size_t size_b)
{
	int32_t res;

	for (uint32_t i = 0; i < size_b / 2; i++) {
		res = ((int16_t *)pcm_a)[i] + ((int16_t *)pcm_b)[i];

		hard_limiter(&res);

		((int16_t *)pcm_a)[i] = (int16_t)res;
	}
}

/* Mix mono into both channels of a stereo buffer */
static void pcm_mix_b_mono_into_a_stereo_lr(void *const pcm_a, size_t size_a,
					    void const *const pcm_b, size_t size_b)
{
	int32_t res;

	/* Use size_b as this is the length of the mono sample.
	 * This must be *2 to traverse the stereo sample and /2 since
	 * the sample is two bytes in size.
	 */
	for (uint32_t i = 0; i < size_b; i++) {
		res = ((int16_t *)pcm_a)[i] + ((int16_t *)pcm_b)[i / 2];

		hard_limiter(&res);

		((int16_t *)pcm_a)[i] = (int16_t)res;
	}
}

/* Mix mono into left channel of a stereo buffer */
static void pcm_mix_b_mono_into_a_stereo_l(void *const pcm_a, size_t size_a,
					   void const *const pcm_b, size_t size_b)
{
	int32_t res;

	for (uint32_t i = 0; i < size_b / 2; i++) {
		res = ((int16_t *)pcm_a)[i * 2] + ((int16_t *)pcm_b)[i];

		hard_limiter(&res);

		((int16_t *)pcm_a)[i * 2] = (int16_t)res;
	}
}

/* Mix mono into right channel of a stereo buffer */
static void pcm_mix_b_mono_into_a_stereo_r(void *const pcm_a, size_t size_a,
					   void const *const pcm_b, size_t size_b)
{
	int32_t res;

	for (uint32_t i = 0; i < size_b / 2; i++) {
		res = ((int16_t *)pcm_a)[i * 2 + 1] + ((int16_t *)pcm_b)[i];

		hard_limiter(&res);

		((int16_t *)pcm_a)[i * 2 + 1] = (int16_t)res;
	}
}

int pcm_mix(void *const pcm_a, size_t size_a, void const *const pcm_b, size_t size_b,
	    enum pcm_mix_mode mix_mode)
{
	if (pcm_a == NULL || size_a == 0) {
		return -EINVAL;
	}

	if (pcm_b == NULL || size_b == 0) {
		/* Nothing to mix, returning */
		return 0;
	}

	switch (mix_mode) {
	case B_STEREO_INTO_A_STEREO:
		/* Fall through */
	case B_MONO_INTO_A_MONO:
		if (size_b > size_a) {
			return -EPERM;
		}
		pcm_mix_identical(pcm_a, size_a, pcm_b, size_b);
		break;
	case B_MONO_INTO_A_STEREO_LR:
		if (size_b > (size_a / 2)) {
			return -EPERM;
		}
		pcm_mix_b_mono_into_a_stereo_lr(pcm_a, size_a, pcm_b, size_b);
		break;
	case B_MONO_INTO_A_STEREO_L:
		if (size_b > (size_a / 2)) {
			LOG_ERR("size a %d size b %d", size_a, size_b);
			return -EPERM;
		}
		pcm_mix_b_mono_into_a_stereo_l(pcm_a, size_a, pcm_b, size_b);
		break;
	case B_MONO_INTO_A_STEREO_R:
		if (size_b > (size_a / 2)) {
			return -EPERM;
		}
		pcm_mix_b_mono_into_a_stereo_r(pcm_a, size_a, pcm_b, size_b);
		break;
	default:
		return -ESRCH;
	};

	return 0;
}

int pcm_mixer(struct audio_data *pcm_a, struct audio_data *pcm_b, enum pcm_mix_mode mix_mode,
			  uint8_t channel_num, bool interleave)
{
	if (pcm_a->data == NULL || pcm_a->data_size == 0) {
		LOG_ERR("pcm_a pointer cannot be zero or data size be zero");
		return -EINVAL;
	}

	if (pcm_b->data == NULL || pcm_b->data_size == 0) {
		LOG_DBG("Nothing to mix, returning");
		return 0;
	}

	if (pcm_a->meta.bits_per_sample != pcm_b->meta.bits_per_sample ||
	 pcm_a->meta.carried_bits_per_sample != pcm_b->meta.carried_bits_per_sample) {
		LOG_ERR("sample/carrier size miss match");
		return -EINVAL;
	}

	if (channel_num == 0) {
		LOG_ERR("number of channels cannot be zero");
		return -EPERM;
	}

#if 0
	samples_per_channel = pcm_a->data_size / pcm_a->meta.carried_bits_per_sample / channel_num;

	if (interleaved) {
		step = channel_num - 1;
		frame_step = step;
	} else {
		step = 0;
		frame_step = bytes_per_channel * (channel_num - 1);
	}

	if (pcm_cont->meta.locations & pcm_finite->meta.locations) {
		out_chan = pcm_finite->meta.locations;
	} else {
		LOG_ERR("pcm_con does not have a channel(s) to extend into: %d   %d", pcm_cont->meta.locations, pcm_finite->meta.locations);
		return -EINVAL;
	}

	chan = 0;

	switch (pcm_a->meta.carried_bits_per_sample) {
	case 24:
		/* Fall through */
	case 32:
		if (pcm_b->data_size > pcm_a->data_size) {
			LOG_ERR("data size missmatch, mode %d", mix_mode);
			return -EPERM;
		}
		pcm_mix_32(pcm_a->data, pcm_a->data_size, pcm_b->data, pcm_b->data_size);
		break;
	case 16:
		if (pcm_b->data_size > (pcm_a->data_size / 2)) {
			LOG_ERR("data size missmatch, mode %d", mix_mode);
			return -EPERM;
		}
		pcm_mix_8(pcm_a->data, pcm_a->data_size, pcm_b->data, pcm_b->data_size);
		break;
	case 8:
		if (pcm_b->data_size > (pcm_a->data_size / 2)) {
			LOG_ERR("data size missmatch, mode %d", mix_mode);
			return -EPERM;
		}
		pcm_mix_16(pcm_a->data, pcm_a->data_size, pcm_b->data, pcm_b->data_size);
		break;
	default:
		return -ESRCH;
	};
#endif
	return 0;
}
