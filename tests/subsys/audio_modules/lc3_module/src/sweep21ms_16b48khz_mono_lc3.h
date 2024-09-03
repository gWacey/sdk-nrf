/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier:  LicenseRef-Nordic-5-Clause
 */

#ifndef _LC3_IN_H_
#define _LC3_IN_H_

#include <stdint.h>

#define LC3_MONO_SIZE (384)

uint8_t lc3_mono[LC3_MONO_SIZE] = {
	0x1C, 0xCC, 0x12, 0x00, 0xE0, 0x01, 0xC0, 0x03, 0x01, 0x00, 0xE8, 0x03, 0x00, 0x00, 0xE8,
	0x03, 0x00, 0x00, 0x78, 0x00, 0x45, 0x0B, 0x24, 0xBE, 0x5E, 0xAC, 0xFA, 0x38, 0xEE, 0x5B,
	0xFD, 0x7C, 0x8B, 0x9E, 0xF8, 0xC9, 0xF7, 0xED, 0xB9, 0x2C, 0x2A, 0xB3, 0x73, 0x7A, 0x1B,
	0x99, 0x1B, 0x65, 0x5E, 0x44, 0x7D, 0x5A, 0x3A, 0xDE, 0x9F, 0x2F, 0xD8, 0x1B, 0x65, 0xE4,
	0x2E, 0x21, 0x8D, 0x82, 0x21, 0x03, 0x35, 0x00, 0x15, 0xA9, 0xDE, 0x67, 0xA1, 0xF0, 0x00,
	0x00, 0x05, 0x5A, 0x78, 0xE2, 0x7F, 0x08, 0xAB, 0xFE, 0x0B, 0x91, 0xEA, 0x8B, 0x7E, 0x9B,
	0x1E, 0xB4, 0xFE, 0xBE, 0x7C, 0xAC, 0xC7, 0x0B, 0xE8, 0x2F, 0xA0, 0x86, 0x31, 0x5F, 0x5F,
	0x47, 0x2A, 0xF0, 0x3C, 0x47, 0x06, 0x52, 0xFB, 0x08, 0x14, 0xF2, 0x36, 0x17, 0xB2, 0x16,
	0x1F, 0x65, 0x08, 0x3B, 0x23, 0x72, 0x3C, 0x16, 0x01, 0x71, 0xC2, 0xA2, 0xBF, 0x14, 0xAA,
	0x33, 0x9C, 0x3A, 0xA6, 0x3C, 0x78, 0x00, 0x6A, 0x58, 0x75, 0x1A, 0x61, 0x1E, 0xBE, 0xE5,
	0xE8, 0x7A, 0x7D, 0xB0, 0x4D, 0x06, 0x71, 0x53, 0xB9, 0x60, 0xF3, 0x10, 0x1A, 0x03, 0xFB,
	0x47, 0x29, 0x23, 0x12, 0x5A, 0x05, 0x01, 0xA5, 0xB4, 0x20, 0x6F, 0xEA, 0xEF, 0x11, 0x39,
	0xE1, 0x0E, 0x13, 0x21, 0x45, 0x62, 0x4F, 0x18, 0xD8, 0xF9, 0x04, 0xD9, 0x70, 0xCA, 0xF0,
	0xF1, 0x61, 0xC7, 0xD2, 0x3D, 0x2D, 0xF6, 0x87, 0xC8, 0xC6, 0x43, 0x16, 0xCD, 0xD5, 0xAB,
	0xF0, 0x4D, 0xBF, 0xFF, 0x00, 0x00, 0x1F, 0xFC, 0x03, 0xF8, 0x1F, 0x83, 0xE1, 0xE0, 0x74,
	0x7F, 0xC4, 0xB9, 0x42, 0x73, 0x22, 0xCC, 0xD6, 0xD0, 0xDC, 0x06, 0xB4, 0xB2, 0x27, 0x4B,
	0xF2, 0x35, 0x3E, 0x21, 0xC2, 0xE5, 0x83, 0x59, 0x65, 0x69, 0x29, 0x8D, 0x65, 0x92, 0xE0,
	0xF9, 0x9C, 0x08, 0xD6, 0x3F, 0x93, 0x84, 0x78, 0x00, 0xA3, 0xDB, 0xB6, 0x61, 0x33, 0x77,
	0xF9, 0x63, 0x88, 0x1B, 0xC6, 0x03, 0x73, 0x20, 0x04, 0xFF, 0xEF, 0xE8, 0x38, 0x01, 0x35,
	0x97, 0x6A, 0x6D, 0x93, 0xDF, 0x1C, 0x26, 0x0E, 0x99, 0x82, 0x14, 0xCB, 0x08, 0xCF, 0x19,
	0x95, 0x4A, 0x6D, 0x13, 0xCD, 0xDF, 0xEF, 0x17, 0xF1, 0xF6, 0xC5, 0xFA, 0x41, 0xA0, 0x9B,
	0x45, 0x6E, 0xCE, 0xD6, 0x87, 0xCF, 0x9E, 0x05, 0x1C, 0x31, 0x38, 0x7B, 0xAB, 0x9B, 0x60,
	0x8F, 0xA2, 0x18, 0xC8, 0x70, 0x81, 0xEA, 0xAE, 0x06, 0xF6, 0x11, 0x45, 0xA2, 0xBA, 0x17,
	0xA5, 0x72, 0x02, 0xAF, 0x2A, 0xA5, 0x56, 0x35, 0x4A, 0x8A, 0x6D, 0x0F, 0xC5, 0x51, 0xA9,
	0x25, 0x5B, 0x1A, 0x94, 0xF1, 0xB7, 0xF1, 0x0C, 0xC0, 0xD4, 0xFB, 0x1D, 0x6B, 0x62, 0xF9,
	0x61, 0x83, 0xB9, 0x3F, 0xE9, 0xF1, 0x3D, 0x75, 0x2C,
};

#endif /* _LC3_IN_H_ */
