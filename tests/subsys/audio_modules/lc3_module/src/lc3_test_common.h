/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include "audio_module/audio_module.h"

#define TEST_SAMPLE_BIT_DEPTH	    16
#define TEST_PCM_BIT_DEPTH	    16
#define TEST_ENC_MONO_BUF_SIZE	    120
#define TEST_ENC_STEREO_BUF_SIZE    (TEST_ENC_MONO_BUF_SIZE * 2)
#define TEST_DEC_MONO_BUF_SIZE	    960
#define TEST_DEC_MONO_BUF_SAMPLES   (TEST_DEC_MONO_BUF_SIZE / (TEST_PCM_BIT_DEPTH / 8))
#define TEST_DEC_STEREO_BUF_SIZE    (TEST_DEC_MONO_BUF_SIZE * 2)
#define TEST_DEC_STEREO_BUF_SAMPLES (TEST_DEC_STEREO_BUF_SIZE / (TEST_PCM_BIT_DEPTH / 8))
#define TEST_PCM_SAMPLE_RATE	    48000
#define TEST_LC3_BITRATE	    96000
#define TEST_LC3_FRAME_SIZE_US	    10000

#define TEST_AUDIO_CHANNELS_MONO      1
#define TEST_AUDIO_CHANNELS_DUAL_MONO 2
#define TEST_AUDIO_CHANNELS_STEREO    2
#define TEST_AUDIO_CHANNELS_MAX	      6

#define TEST_AUDIO_MONO_LEFT_LOCATIONS	   (1 << AUDIO_CH_L)
#define TEST_AUDIO_MONO_RIGHT_LOCATIONS	   (1 << AUDIO_CH_R)
#define TEST_AUDIO_STEREO_LOCATIONS	   ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))
#define TEST_AUDIO_MULTI_CHANNEL_LOCATIONS ((1 << AUDIO_CH_L) | (1 << AUDIO_CH_R))

#define TEST_ENC_MULTI_BUF_SIZE (TEST_ENC_MONO_BUF_SIZE * TEST_AUDIO_CHANNELS_MAX)
#define TEST_DEC_MULTI_BUF_SIZE (TEST_DEC_MONO_BUF_SIZE * TEST_AUDIO_CHANNELS_MAX)

#endif /* _COMMON_H_ */
