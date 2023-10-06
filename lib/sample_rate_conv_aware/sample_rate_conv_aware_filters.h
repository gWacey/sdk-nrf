/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <dsp/filtering_functions.h>

#define AWARE_FILTER_48KHZ_16KHZ_SMALL_TAP_NUM 81
q15_t aware_filter_48khz_16khz_small[AWARE_FILTER_48KHZ_16KHZ_SMALL_TAP_NUM] = {
	396,  523,  423,   -68,	  -764,	 -1266, -1247, -729,  -100,  190,  -3,	 -396, -550,  -277,
	176,  372,  119,   -324,  -490,	 -174,	334,   512,   140,   -437, -617, -151, 545,   743,
	144,  -729, -958,  -145,  1041,	 1349,	144,   -1725, -2324, -146, 4440, 9105, 11069, 9105,
	4440, -146, -2324, -1725, 144,	 1349,	1041,  -145,  -958,  -729, 144,	 743,  545,   -151,
	-617, -437, 140,   512,	  334,	 -174,	-490,  -324,  119,   372,  176,	 -277, -550,  -396,
	-3,   190,  -100,  -729,  -1247, -1266, -764,  -68,   423,   523,  396};

#define AWARE_FILTER_48KHZ_16KHZ_BIG_TAP_NUM 180
q15_t aware_filter_48khz_16khz_big[AWARE_FILTER_48KHZ_16KHZ_BIG_TAP_NUM] = {
	93,    142,   154,   79,   -62,	 -199,	-250,  -192, -76,  8,	  3,	 -68,	-127, -111,
	-30,   41,    39,    -31,  -97,	 -89,	-10,   66,   65,   -14,	  -94,	 -92,	-4,   86,
	88,    -4,    -102,  -106, -6,	 104,	113,   6,    -116, -129,  -13,	 123,	142,  17,
	-134,  -160,  -25,   144,  177,	 32,	-156,  -199, -42,  169,	  223,	 53,	-184, -251,
	-66,   201,   284,   82,   -221, -323,	-102,  244,  370,  126,	  -272,	 -429,	-157, 308,
	504,   197,   -355,  -606, -253, 420,	751,   336,  -516, -977,  -469,	 679,	1379, 725,
	-1016, -2311, -1414, 2157, 6951, 10357, 10357, 6951, 2157, -1414, -2311, -1016, 725,  1379,
	679,   -469,  -977,  -516, 336,	 751,	420,   -253, -606, -355,  197,	 504,	308,  -157,
	-429,  -272,  126,   370,  244,	 -102,	-323,  -221, 82,   284,	  201,	 -66,	-251, -184,
	53,    223,   169,   -42,  -199, -156,	32,    177,  144,  -25,	  -160,	 -134,	17,   142,
	123,   -13,   -129,  -116, 6,	 113,	104,   -6,   -106, -102,  -4,	 88,	86,   -4,
	-92,   -94,   -14,   65,   66,	 -10,	-89,   -97,  -31,  39,	  41,	 -30,	-111, -127,
	-68,   3,     8,     -76,  -192, -250,	-199,  -62,  79,   154,	  142,	 93};

#define AWARE_FILTER_48KHZ_24KHZ_BIG_TAP_NUM 122
q15_t aware_filter_48khz_24khz_big[AWARE_FILTER_48KHZ_24KHZ_BIG_TAP_NUM] = {
	288,  1280,  1709,  1050, -243,	 -691,	-39,  485,   110,   -374, -129, 310,   133,   -274,
	-132, 254,   132,   -243, -133,	 239,	133,  -242,  -140,  245,  145,	-255,  -154,  265,
	165,  -277,  -178,  293,  193,	 -311,	-212, 332,   233,   -358, -259, 389,   291,   -425,
	-330, 470,   379,   -525, -441,	 597,	524,  -693,  -637,  829,  803,	-1038, -1070, 1397,
	1575, -2165, -2888, 4976, 14690, 14690, 4976, -2888, -2165, 1575, 1397, -1070, -1038, 803,
	829,  -637,  -693,  524,  597,	 -441,	-525, 379,   470,   -330, -425, 291,   389,   -259,
	-358, 233,   332,   -212, -311,	 193,	293,  -178,  -277,  165,  265,	-154,  -255,  145,
	245,  -140,  -242,  133,  239,	 -133,	-243, 132,   254,   -132, -274, 133,   310,   -129,
	-374, 110,   485,   -39,  -691,	 -243,	1050, 1709,  1280,  288};
