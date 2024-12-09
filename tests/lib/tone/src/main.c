/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>
#include <zephyr/tc_util.h>
#include <tone.h>

static int32_t tone_sum(int16_t *tone, size_t size)
{
	int32_t sum = 0;

	for (size_t i = 0; i < size / 2; i++) {
		sum += tone[i];
	}

	return sum;
}

static void tone_high_low_idx(int16_t *tone, size_t size, uint32_t *idx_low, uint32_t *idx_high)
{
	int16_t highest = 0;
	int16_t lowest = 0;

	for (size_t i = 0; i < size / 2; i++) {
		if (tone[i] < lowest) {
			lowest = tone[i];
			*idx_low = i;
		}
		if (tone[i] > highest) {
			highest = tone[i];
			*idx_high = i;
		}
	}
}

/* Get the index of the lowest and highest sample */
ZTEST(suite_tone, test_tone_gen_valid)
{
#define NUM_TESTS 3
	uint16_t freq[] = {100, 480, 960};
	uint32_t smpl_freq[] = {10000, 48000, 48000};
	size_t tone_size_desired[] = {200, 200, 100};
	float amplitude[] = {1, 1, 1};

	for (uint8_t i = 0; i < NUM_TESTS; i++) {
		int16_t tone[400] = {0};
		size_t tone_size = 0;
		uint32_t idx_low = 0;
		uint32_t idx_high = 0;

		zassert_equal(tone_gen(tone, &tone_size, freq[i], smpl_freq[i], amplitude[i]), 0,
			      "Err code returned");
		zassert_equal(tone_size, tone_size_desired[i], "Incorrect tone size");

		/* Since a single period is generated, the center sample should always be zero */
		zassert_equal(tone[tone_size / 2], 0, "Center sample not zero");
		zassert_equal(tone_sum(tone, tone_size), 0, "The sum of samples are not zero");

		tone_high_low_idx(tone, tone_size, &idx_low, &idx_high);
		zassert_equal(idx_low, 3 * (tone_size / 2) / 4,
			      "Lowest sample not at the 3/4 mark");
		zassert_equal(idx_high, (tone_size / 2) / 4, "Highest sample not at the 1/4 mark");
	}
}

ZTEST(suite_tone, test_illegal_args)
{
	int16_t tone[200] = {0};
	size_t tone_size;
	uint16_t freq = 100;
	uint32_t smpl_freq = 10000;
	float amplitude = 1;

	/* NULL ptr */
	zassert_equal(tone_gen(NULL, &tone_size, freq, smpl_freq, amplitude), -ENXIO,
		      "Wrong code returned");
	/* NULL ptr */
	zassert_equal(tone_gen(tone, NULL, freq, smpl_freq, amplitude), -ENXIO,
		      "Wrong code returned");
	/* 0 freq */
	zassert_equal(tone_gen(tone, &tone_size, 0, smpl_freq, amplitude), -EINVAL,
		      "Wrong code returned");
	/* freq too low */
	zassert_equal(tone_gen(tone, &tone_size, 10, smpl_freq, amplitude), -EINVAL,
		      "Wrong code returned");
	/* freq too high */
	zassert_equal(tone_gen(tone, &tone_size, 10001, smpl_freq, amplitude), -EINVAL,
		      "Wrong code returned");
	/* smpl_freq 0 */
	zassert_equal(tone_gen(tone, &tone_size, freq, 0, amplitude), -EINVAL, "Err code returned");
	/* 0 Amplitude */
	zassert_equal(tone_gen(tone, &tone_size, freq, smpl_freq, 0), -EPERM, "Err code returned");
	/* Amplitude too high*/
	zassert_equal(tone_gen(tone, &tone_size, freq, smpl_freq, 1.1), -EPERM,
		      "Err code returned");
}

void test_tone_sample_width(uint8_t samp_width, uint8_t *carrier)
{
	uint16_t freq[] = {100, 480, 960};
	uint32_t smpl_freq[] = {10000, 48000, 48000};
	size_t tone_size_desired[] = {200, 200, 100};
	float amplitude[] = {1, 1, 1};

	for (uint8_t i = 0; i < ARRAY_SIZE(carrier); i++) {
		int16_t tone[400] = {0};
		size_t tone_size = 0;
		uint32_t idx_low = 0;
		uint32_t idx_high = 0;

		zassert_equal(tone_gen(tone, &tone_size, freq[i], smpl_freq[i], samp_width,
				       carrier[i], amplitude[i]),
			      0, "Err code returned");
		zassert_equal(tone_size, tone_size_desired[i], "Incorrect tone size");

		/* Since a single period is generated, the center sample should always be zero */
		zassert_equal(tone[tone_size / 2], 0, "Center sample not zero");
		zassert_equal(tone_sum(tone, tone_size), 0, "The sum of samples are not zero");

		tone_high_low_idx(tone, tone_size, &idx_low, &idx_high);
		zassert_equal(idx_low, 3 * (tone_size / 2) / 4,
			      "Lowest sample not at the 3/4 mark");
		zassert_equal(idx_high, (tone_size / 2) / 4, "Highest sample not at the 1/4 mark");
	}
}

/* Get the index of the lowest and highest sample */
ZTEST(suite_tone, test_tone_gen_size_valid)
{
	uint8_t sample_width[] = {8, 12, 16, 32};
	uint8_t carrier_samp_8[] = {8, 16, 32};
	uint8_t carrier_samp_12[] = {16, 32};
	uint8_t carrier_samp_16[] = {16, 32};
	uint8_t carrier_samp_32[] = {32};
	uint8_t *carrier[] = {carrier_samp_8, carrier_samp_12, carrier_samp_16, carrier_samp_32};

	for (size_t i = 0; ARRAY_SIZE(sample_width); i++) {
		test_tone_sample_width(samp_width[i], carrier[i]);
	}
}

ZTEST(suite_tone_gen_size, test_tone_gen_size_illegal_args)
{
	int16_t tone[200] = {0};
	size_t tone_size;
	uint16_t freq = 100;
	uint32_t smpl_freq = 10000;
	uint8_t sample_bits = 16;
	uint_t carrier = 32;
	float amplitude = 1;

	/* NULL ptr */
	zassert_equal(
		tone_gen_size(NULL, &tone_size, freq, smpl_freq, sample_bits, carrier, amplitude),
		-ENXIO, "Wrong code returned");
	/* NULL ptr */
	zassert_equal(tone_gen_size(tone, NULL, freq, smpl_freq, sample_bits, carrier, amplitude),
		      -ENXIO, "Wrong code returned");
	/* 0 freq */
	zassert_equal(
		tone_gen_size(tone, &tone_size, 0, smpl_freq, sample_bits, carrier, amplitude),
		-EINVAL, "Wrong code returned");
	/* freq too low */
	zassert_equal(
		tone_gen_size(tone, &tone_size, 10, smpl_freq, sample_bits, carrier, amplitude),
		-EINVAL, "Wrong code returned");
	/* freq too high */
	zassert_equal(
		tone_gen_size(tone, &tone_size, 10001, smpl_freq, sample_bits, carrier, amplitude),
		-EINVAL, "Wrong code returned");
	/* smpl_freq 0 */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, 0, sample_bits, carrier, amplitude),
		      -EINVAL, "Err code returned");
	/* sample_bits 0 */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, 0, carrier, amplitude),
		      -EINVAL, "Err code returned");
	/* sample_bits not in set */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, 12, carrier, amplitude),
		      -EINVAL, "Err code returned");
	/* carrier 0 */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, sample_bits, 0, amplitude),
		      -EINVAL, "Err code returned");
	/* carrier too small */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, sample_bits, 8, amplitude),
		      -EINVAL, "Err code returned");
	/* carrier not in set */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, sample_bits, 26, amplitude),
		      -EINVAL, "Err code returned");
	/* 0 Amplitude */
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, sample_bits, carrier, 0),
		      -EPERM, "Err code returned");
	/* Amplitude too high*/
	zassert_equal(tone_gen_size(tone, &tone_size, freq, smpl_freq, sample_bits, carrier, 1.1),
		      -EPERM, "Err code returned");
}

ZTEST_SUITE(suite_tone, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(suite_tone_gen_size, NULL, NULL, NULL, NULL, NULL);
