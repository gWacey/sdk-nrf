/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include "amod_api.h"

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
#define TEST_AUDIO_CH_L		    0
#define TEST_AUDIO_CH_R		    1
#define TEST_AUDIO_CHANNELS_MAX	    5

#endif /* _COMMON_H_ */
