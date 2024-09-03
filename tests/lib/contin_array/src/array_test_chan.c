/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>
#include <zephyr/tc_util.h>
#include <audio_defines.h>
#include <contin_array.h>
#include "array_test_data.h"

#define CONTIN_TEST_DATA_SIZE	  (3)
#define CONTIN_TEST_CHAN_LOCS	  (CONTIN_TEST_DATA_SIZE + 1)
#define CONTIN_TEST_CHAN_LOCS_MAX (32)

#define TEST_CONTIN_AUDIO_DATA(ad1, ad2, d1, d2, ds1, ds2)                                         \
	(ad1).data = (d1);                                                                         \
	(ad2).data = (d2);                                                                         \
	(ad1).data_size = (ds1);                                                                   \
	(ad2).data_size = (ds2);

struct audio_metadata test_meta = {.data_coding = PCM,
				   .data_len_us = 10000,
				   .sample_rate_hz = 48000,
				   .bits_per_sample = 16,
				   .carried_bits_per_sample = 16,
				   .locations = 0x00000001,
				   .reference_ts_us = 0,
				   .data_rx_ts_us = 0,
				   .bad_data = false};

/* Test parameter validation */
ZTEST(suite_contin_array_chan, test_contin_array_api)
{
	int ret;
	struct audio_data pcm_cont;
	struct audio_data pcm_finite;
	uint8_t data[CONTIN_TEST_DATA_SIZE];
	uint16_t data_size = sizeof(data);
	uint8_t channels = CONTIN_TEST_CHAN_LOCS;
	bool interleaved = false;
	uint32_t finite_pos;

	/* Test for pcm_cont pointer NULL */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(NULL, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -ENXIO, "Failed to recognise NULL pointer: %d", ret);

	/* Test for pcm_finite pointer NULL */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, NULL, channels, interleaved, &finite_pos);
	zassert_equal(ret, -ENXIO, "Failed to recognise NULL pointer: %d", ret);

	/* Test for pcm_cont.data pointer NULL */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, NULL, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -ENXIO, "Failed to recognise NULL pointer: %d", ret);

	/* Test for pcm_finite.data pointer NULL */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, NULL, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -ENXIO, "Failed to recognise NULL pointer: %d", ret);

	/* Test pcm_cont.bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_cont.meta.bits_per_sample = 8;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to recognise NULL pointer: %d", ret);

	/* Test pcm_cont.meta.bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_cont.meta.bits_per_sample = 32;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to recognise NULL pointer: %d", ret);

	/* Test pcm_finite.bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_finite.meta.bits_per_sample = 8;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to detect bits per sample difference: %d", ret);

	/* Test pcm_finite.meta.bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_finite.meta.bits_per_sample = 32;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to detect bits per sample difference: %d", ret);

	/* Test pcm_finite.carried_bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_cont.meta.carried_bits_per_sample = 8;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to detect bits per sample difference: %d", ret);

	/* Test pcm_finite.meta.carried_bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_cont.meta.carried_bits_per_sample = 32;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to detect bits per sample difference: %d", ret);

	/* Test pcm_finite.carried_bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_finite.meta.carried_bits_per_sample = 8;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to data size of 0: %d", ret);

	/* Test pcm_finite.meta.carried_bits_per_sample */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	pcm_finite.meta.carried_bits_per_sample = 32;
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to data size of 0: %d", ret);

	/* Test for pcm_cont.data_size 0 */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, 0, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to data size of 0: %d", ret);

	/* Test for pcm_finite.data_size 0 */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, 0);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, channels, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to data size of 0: %d", ret);

	/* Test for 0 output location */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, 0, interleaved, &finite_pos);
	zassert_equal(ret, -EINVAL, "Failed to there are 0 output locations: %d", ret);

	/* Test for too many output location */
	TEST_CONTIN_AUDIO_DATA(pcm_cont, pcm_finite, data, data, data_size, data_size);
	memcpy(&pcm_cont.meta, &test_meta, sizeof(struct audio_metadata));
	memcpy(&pcm_finite.meta, &test_meta, sizeof(struct audio_metadata));
	ret = contin_array_chans_create(&pcm_cont, &pcm_finite, CONTIN_TEST_CHAN_LOCS, interleaved,
					&finite_pos);
	zassert_equal(ret, -EINVAL, "Failed too many output locations: %d", ret);
}

int test_contin_array_chans(struct audio_data *pcm_cont, struct audio_data *pcm_finite,
			    uint8_t channels, bool interleaved, uint32_t *const finite_pos,
			    uint32_t iterations)
{
	int ret;
	const size_t const_arr_end_loc;

	for (int i = 0; i < iterations; i++) {
		pcm_finite->data_size -= j;
		const_arr_end_loc = pcm_finite->data_size - 1;

		ret = contin_array_chans_create(pcm_cont, pcm_finite, channels, interleaved,
						finite_pos);
		zassert_equal(ret, 0, "contin_array_chans_create did not return zero: %d", ret);

		for (int j = 0; j < channels; j++) {

			zassert_equal(pcm_cont->data[j], test_arr[0],
				      "First value is not identical");
			zassert_equal(pcm_cont->data[const_arr_end_loc],
				      pcm_finite->data[const_arr_end_loc],
				      "Last value is not identical 0x%x 0x%x",
				      pcm_cont->data[const_arr_end_loc],
				      pcm_finite->data[const_arr_end_loc]);
		}
	}
}

/* Test with const array size being shorter than contin array size */
ZTEST(suite_contin_array_chan, test_simp_arr_loop_short)
{
	const uint32_t ITERATIONS = 2000;
	const size_t CONTIN_ARR_SIZE = 97; /* Test with random "uneven" value */
	const uint32_t CONTIN_LAST_VAL_IDX = (CONTIN_ARR_SIZE - 1);
	/* Test with random "uneven" value, shorter than CONTIN_ARR_SIZE */
	const size_t const_arr_size = 44;
	char contin_arr[CONTIN_ARR_SIZE];
	int ret;

	char contin_first_val;
	char contin_last_val;
	uint32_t finite_pos = 0;

	for (int i = 0; i < ITERATIONS; i++) {
		ret = contin_array_create(contin_arr, CONTIN_ARR_SIZE, test_arr, const_arr_size,
					  &finite_pos);
		zassert_equal(ret, 0, "contin_array_create did not return zero");

		if (i == 0) { /* First run */
			zassert_equal(contin_arr[0], test_arr[0], "First value is not identical");
			zassert_equal(contin_arr[const_arr_size], test_arr[0],
				      "First repetition value is not identical");
		} else {
			/* The last val is the last element of the test_arr */
			if (contin_last_val == test_arr[const_arr_size - 1]) {
				zassert_equal(contin_arr[0], test_arr[0],
					      "First value is not identical after wrap");
			} else {
				zassert_equal(contin_arr[0], contin_last_val + 1,
					      "Last value is not identical. contin_arr[0]: 0x%04x, "
					      "contin_last_val - 1: 0x%04x\n",
					      contin_arr[0], contin_last_val + 1);
			}
		}

		contin_first_val = contin_arr[0];
		contin_last_val = contin_arr[CONTIN_LAST_VAL_IDX];
	}
}
