/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief PCM audio mixer library header.
 */

#ifndef _PCM_MIX_H_
#define _PCM_MIX_H_

#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

/**
 * @defgroup pcm_mix Pulse Code Modulation mixer
 * @brief Pulse Code Modulation audio mixer library.
 *
 * @{
 */

enum pcm_mix_mode {
	B_STEREO_INTO_A_STEREO,
	B_MONO_INTO_A_MONO,
	B_MONO_INTO_A_STEREO_LR,
	B_MONO_INTO_A_STEREO_L,
	B_MONO_INTO_A_STEREO_R,
	B_ALL_INTO_A_ALL,
	B_MONO_INTO_A_ALL,
	B_MONO_INTO_A_CHAN,
	B_CHAN_INTO_A_CHAN,
};

/**
 * @brief Mixes two buffers of PCM data.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or stereo as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a stereo buffer.
 * Hard coded for the signed 16-bit PCM.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param size_a        [in]     Size of the PCM data buffer A (in bytes).
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param size_b        [in]     Size of the PCM data buffer B (in bytes).
 * @param mix_mode      [in]     Mixing mode according to pcm_mix_mode.
 *
 * @retval 0            Success. Result stored in pcm_a.
 * @retval -EINVAL      pcm_a is NULL or size_a = 0.
 * @retval -EPERM       Either size_b < size_a (for stereo to stereo, mono to mono)
 *			or size_a/2 < size_b (for mono to stereo mix).
 * @retval -ESRCH       Invalid mixing mode.
 */
int pcm_mix(void *const pcm_a, size_t size_a, void const *const pcm_b, size_t size_b,
	    enum pcm_mix_mode mix_mode);

/**
 * @brief Mixes two buffers of PCM data.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param mix_mode      [in]     Mixing mode according to pcm_mix_mode.
 *
 * @retval  0           Success. Result stored in pcm_a.
 * @retval -ENXIO       One of the pointers is NULL.
 * @retval -EPERM       A buffer is has an incorrect size.
 * @retval -EINVAL      There is a mismatch in the meta data.
 */
int pcm_mixer(struct net_buf *pcm_a, struct net_buf *pcm_b, enum pcm_mix_mode mix_mode);

/**
 * @brief Mixes two channels together from two multi-channel buffers of PCM data.
 *
 * @note Uses simple addition with hard clip protection.
 * Input can be mono or multi-channel as long as the inputs match.
 * By selecting the mix mode, mono can also be mixed into a multi-channel buffer.
 *
 * @param pcm_a         [in/out] Pointer to the PCM data buffer A.
 * @param pcm_b         [in]     Pointer to the PCM data buffer B.
 * @param out_ch        [in]     Channel in PCM data buffer A to mix into.
 * @param in_ch         [in]     Channel in PCM data buffer B to mix into PCM data buffer A.
 * @param mix_mode      [in]     Mixing mode according to pcm_mix_mode.
 *
 * @retval  0           Success. Result stored in pcm_a.
 * @retval -ENXIO       One of the pointers is NULL.
 * @retval -EPERM       A buffer is has an incorrect size.
 * @retval -EINVAL      There is a mismatch in the meta data.
 * @retval -ESRCH       Does not support the mixer mode or carrier bits size.
 */
int pcm_mixer_chans(struct net_buf *pcm_a, struct net_buf *pcm_b, uint8_t out_ch, uint8_t in_ch,
		    enum pcm_mix_mode mix_mode);

/**
 * @}
 */
#endif /* _PCM_MIX_H_ */
