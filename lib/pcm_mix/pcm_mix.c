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
static void pcm_mix_identical(void *const pcm_a, void const *const pcm_b, size_t size_b)
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
		pcm_mix_identical(pcm_a, pcm_b, size_b);
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

/**
 * @brief Mixes two 32-bit buffers of PCM data together.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param start         [in]     Starting range of channels to mix.
 * @param end           [in]     End of the range of channels to mix.
 * @param step_a        [in]     The sample step size for PCM buffer A.
 * @param step_b        [in]     The sample step size for PCM buffer B.
 * @param chan_start_a  [in]     The starting sample for PCM buffer A.
 * @param chan_start_b  [in]     The starting sample for PCM buffer B.
 * @param bytes_per_loc [in]     The number of bytes per channel for PCM buffer A.
 */
static void mix_32(int32_t *pcm_a, int32_t *pcm_b, uint8_t start, uint8_t end, uint16_t step_a,
		   uint16_t step_b, uint16_t chan_start_a, uint16_t chan_start_b,
		   size_t bytes_per_location)
{
	int64_t res;
	int32_t *a_in, *b_in;

	for (size_t i = start; i < end; i++) {
		a_in = &pcm_a[i * chan_start_a];
		b_in = &pcm_b[i * chan_start_b];

		for (size_t j = 0; j < bytes_per_location; j++) {
			res = *a_in + *b_in;
			b_in += step_b;

			if (res < INT32_MIN) {
				LOG_DBG("Clip");
				*a_in = INT32_MIN;
			} else if (res > INT32_MAX) {
				LOG_DBG("Clip");
				*a_in = INT32_MAX;
			} else {
				*a_in = (int32_t)res;
			}

			a_in += step_a;
		}
	}
}

/**
 * @brief Mixes two 16-bit buffers of PCM data together.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param start         [in]     Starting range of channels to mix.
 * @param end           [in]     End of the range of channels to mix.
 * @param step_a        [in]     The sample step size for PCM buffer A.
 * @param step_b        [in]     The sample step size for PCM buffer B.
 * @param chan_start_a  [in]     The starting sample for PCM buffer A.
 * @param chan_start_b  [in]     The starting sample for PCM buffer B.
 * @param bytes_per_loc [in]     The number of bytes per channel for PCM buffer A.
 */
static void mix_16(int16_t *pcm_a, int16_t *pcm_b, uint8_t start, uint8_t end, uint16_t step_a,
		   uint16_t step_b, uint16_t chan_start_a, uint16_t chan_start_b,
		   size_t bytes_per_location)
{
	int32_t res;
	int16_t *a_in, *b_in;

	for (size_t i = start; i < end; i++) {
		a_in = &pcm_a[i * chan_start_a];
		b_in = &pcm_b[i * chan_start_b];

		for (size_t j = 0; j < bytes_per_location; j++) {
			res = *a_in + *b_in;
			b_in += step_b;

			if (res < INT16_MIN) {
				LOG_DBG("Clip");
				*a_in = INT16_MIN;
			} else if (res > INT16_MAX) {
				LOG_DBG("Clip");
				*a_in = INT16_MAX;
			} else {
				*a_in = (int16_t)res;
			}

			a_in += step_a;
		}
	}
}

/**
 * @brief Mixes two 8-bit buffers of PCM data together.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param start         [in]     Starting range of channels to mix.
 * @param end           [in]     End of the range of channels to mix.
 * @param step_a        [in]     The sample step size for PCM buffer A.
 * @param step_b        [in]     The sample step size for PCM buffer B.
 * @param chan_start_a  [in]     The starting sample for PCM buffer A.
 * @param chan_start_b  [in]     The starting sample for PCM buffer B.
 * @param bytes_per_loc [in]     The number of bytes per channel for PCM buffer A.
 */
static void mix_8(int8_t *pcm_a, int8_t *pcm_b, uint8_t start, uint8_t end, uint16_t step_a,
		  uint16_t step_b, uint16_t chan_start_a, uint16_t chan_start_b,
		  size_t bytes_per_location)
{
	int16_t res;
	int8_t *a_in, *b_in;

	for (size_t i = start; i < end; i++) {
		a_in = &pcm_a[i * chan_start_a];
		b_in = &pcm_b[i * chan_start_b];

		for (size_t j = 0; j < bytes_per_location; j++) {
			res = *a_in + *b_in;
			b_in += step_b;

			if (res < INT8_MIN) {
				LOG_DBG("Clip");
				res = INT8_MIN;
			} else if (res > INT8_MAX) {
				LOG_DBG("Clip");
				res = INT8_MAX;
			}

			*a_in = (int8_t)res;
			a_in += step_a;
		}
	}
}

/**
 * @brief Mixes two buffers of PCM data together.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param out_ch        [in]     Channel in buffer A to mix into, 0 if unused.
 * @param in_ch         [in]     Channel in buffer B to mix into buffer A, 0 if unused.
 * @param mix_mode      [in]     Mixing mode according to pcm_mix_mode.
 *
 * @retval  0           Success. Result stored in pcm_a.
 * @retval -ENXIO       One of the pointers is NULL.
 * @retval -EPERM       A buffer is has an incorrect size.
 * @retval -EINVAL      There is a mismatch in the meta data.
 * @retval -ESRCH       Does not support the mixer mode or carrier bits size.
 */
static int pcm_mix_ch(struct net_buf *pcm_a, struct net_buf *pcm_b, uint8_t out_ch, uint8_t in_ch,
		      enum pcm_mix_mode mix_mode)
{
	struct audio_metadata *meta_a, *meta_b;
	uint16_t step_a, step_b;
	uint16_t chan_start_a, chan_start_b;
	uint8_t num_ch_a, num_ch_b;
	uint8_t start, end;

	if (pcm_a == NULL || pcm_b == NULL) {
		LOG_ERR("pointer cannot be NULL");
		return -ENXIO;
	}

	if (pcm_a->data == NULL || pcm_b->data == NULL) {
		LOG_ERR("pointer cannot be NULL");
		return -ENXIO;
	}

	if (pcm_a->len == 0 || pcm_b->len == 0) {
		LOG_ERR("data size error");
		return -EPERM;
	}

	meta_a = net_buf_user_data(pcm_a);
	if (meta_a == NULL) {
		LOG_ERR("no metadata");
		return -ENXIO;
	}

	meta_b = net_buf_user_data(pcm_b);
	if (meta_b == NULL) {
		LOG_ERR("no metadata");
		return -ENXIO;
	}

	if (meta_a->sample_rate_hz != meta_b->sample_rate_hz ||
	    meta_a->bytes_per_location != meta_b->bytes_per_location ||
	    meta_a->bits_per_sample != meta_b->bits_per_sample ||
	    meta_a->carried_bits_per_sample != meta_b->carried_bits_per_sample) {
		LOG_ERR("sample/carrier size miss match");
		return -EINVAL;
	}

	num_ch_a = metadata_num_ch_get(meta_a);
	if (num_ch_a == 0) {
		return -EINVAL;
	}

	num_ch_b = metadata_num_ch_get(meta_b);
	if (num_ch_b == 0) {
		return -EINVAL;
	}

	if (meta_a->interleaved) {
		step_a = num_ch_a - 1;
		chan_start_a = 1;
	} else {
		step_a = 0;
		chan_start_a = meta_a->bytes_per_location / (meta_a->carried_bits_per_sample / 8);
	}

	if (meta_b->interleaved) {
		step_b = num_ch_b - 1;
		chan_start_b = 1;
	} else {
		step_b = 0;
		chan_start_b = meta_b->bytes_per_location / (meta_b->carried_bits_per_sample / 8);
	}

	start = 0;
	end = num_ch_a;

	switch (mix_mode) {
	case B_ALL_INTO_A_ALL:
	case B_STEREO_INTO_A_STEREO:
	case B_MONO_INTO_A_MONO:
		if (num_ch_a != num_ch_b) {
			return -EINVAL;
		}
		break;
	case B_MONO_INTO_A_STEREO_LR:
	case B_MONO_INTO_A_STEREO_L:
	case B_MONO_INTO_A_STEREO_R:
		if (num_ch_a == 2) {
			return -EINVAL;
		}
	/* Fall through */
	case B_MONO_INTO_A_ALL:
		if (num_ch_b != 1) {
			return -EINVAL;
		}
	/* Fall through */
	case B_CHAN_INTO_A_CHAN:
		start = 1;
		end = 2;

		if (mix_mode == B_CHAN_INTO_A_CHAN) {
			if (num_ch_a < out_ch || num_ch_b < in_ch) {
				return -EINVAL;
			}

			chan_start_a = chan_start_a * (out_ch - 1);
			chan_start_b = chan_start_b * (in_ch - 1);
		} else {
			chan_start_a = 0;
			chan_start_b = 0;

			if (mix_mode == B_MONO_INTO_A_STEREO_R) {
				chan_start_a = chan_start_a;
			}
		}
		break;
	default:
		return -ESRCH;
	};

	switch (meta_a->carried_bits_per_sample) {
	case 32:
		mix_32((int32_t *)pcm_a->data, (int32_t *)pcm_b->data, start, end, step_a, step_b,
		       chan_start_a, chan_start_b, meta_a->bytes_per_location);
		break;
	case 16:
		mix_16((int16_t *)pcm_a->data, (int16_t *)pcm_b->data, start, end, step_a, step_b,
		       chan_start_a, chan_start_b, meta_a->bytes_per_location);
		break;
	case 8:
		mix_8((int8_t *)pcm_a->data, (int8_t *)pcm_b->data, start, end, step_a, step_b,
		      chan_start_a, chan_start_b, meta_a->bytes_per_location);
		break;
	default:
		return -ESRCH;
	};

	return 0;
}

int pcm_mixer(struct net_buf *pcm_a, struct net_buf *pcm_b, enum pcm_mix_mode mix_mode)
{
	return pcm_mix_ch(pcm_a, pcm_b, 0, 0, mix_mode);
}

int pcm_mixer_chans(struct net_buf *pcm_a, struct net_buf *pcm_b, uint8_t out_ch, uint8_t in_ch,
		    enum pcm_mix_mode mix_mode)
{
	return pcm_mix_ch(pcm_a, pcm_b, out_ch, in_ch, mix_mode);
}
