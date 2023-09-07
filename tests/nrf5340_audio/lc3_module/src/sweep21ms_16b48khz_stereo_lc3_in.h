/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier:  LicenseRef-Nordic-5-Clause
 */

#ifndef _LC3_STEREO_IN_H_
#define _LC3_STEREO_IN_H_

#include <stdint.h>

#define LC3_STEREO_IN_SIZE (384)

uint8_t lc3_stereo_in[LC3_STEREO_IN_SIZE] = {
	0x1C, 0xCC, 0x12, 0x00, 0xE0, 0x01, 0xC0, 0x03, 0x02, 0x00, 0xE8, 0x03, 0x00, 0x00, 0xE8,
	0x03, 0x00, 0x00, 0x78, 0x00, 0x45, 0x06, 0x17, 0xCC, 0x03, 0xD7, 0x05, 0x57, 0x61, 0xD3,
	0x97, 0x12, 0x46, 0x25, 0xB5, 0x98, 0x48, 0xDC, 0x2E, 0x1B, 0xBE, 0x32, 0x70, 0xD6, 0x37,
	0x4B, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xDA, 0xDD, 0xB6, 0x5D, 0x78, 0xF8, 0x1F, 0x21,
	0x6A, 0x31, 0xBE, 0x90, 0xC7, 0xFE, 0x5C, 0xC8, 0xF2, 0x73, 0x13, 0x50, 0xC3, 0x31, 0x7A,
	0xE6, 0x1C, 0x9B, 0x56, 0x04, 0xC4, 0xEB, 0x65, 0x31, 0xA3, 0x3D, 0x68, 0x67, 0x67, 0xAF,
	0xBD, 0xF3, 0xE2, 0xD6, 0xC0, 0xE0, 0x89, 0x19, 0x7F, 0x97, 0x75, 0x99, 0x95, 0xFE, 0xD5,
	0x0F, 0x91, 0xD1, 0xD6, 0x99, 0xA3, 0x06, 0x7E, 0xD9, 0x34, 0xAE, 0x77, 0xA6, 0x90, 0x46,
	0xDE, 0xD5, 0xB6, 0x63, 0x8E, 0x38, 0x7C, 0x08, 0xFF, 0xFE, 0x7F, 0x74, 0xC9, 0xB2, 0x4C,
	0xED, 0xB3, 0x3F, 0x72, 0x9C, 0x78, 0x00, 0x6A, 0x58, 0x8A, 0x71, 0x6D, 0x97, 0x8E, 0x77,
	0x55, 0x6D, 0xEE, 0x7A, 0x1F, 0x33, 0xFC, 0xC9, 0xC0, 0xE3, 0x43, 0x2A, 0xEA, 0x34, 0x1F,
	0xB7, 0xA7, 0xF8, 0xE8, 0x0B, 0xA5, 0x68, 0xA6, 0x94, 0xDD, 0xFF, 0x0C, 0x0D, 0x39, 0xA7,
	0x20, 0xCE, 0x31, 0x99, 0x99, 0x99, 0x96, 0x6B, 0x85, 0x5C, 0x67, 0x76, 0x76, 0x15, 0xCC,
	0xF8, 0x06, 0x89, 0xC6, 0x3F, 0xB3, 0x84, 0x61, 0x59, 0xDA, 0x91, 0xEE, 0x03, 0x2B, 0xBD,
	0x77, 0x2F, 0x68, 0x88, 0x61, 0xD6, 0x74, 0xA3, 0xF2, 0x87, 0x69, 0x03, 0x07, 0xB1, 0x62,
	0xA1, 0xE3, 0x41, 0x4C, 0xA1, 0xD8, 0x6C, 0x5C, 0x4B, 0xE8, 0xA5, 0x3C, 0xCD, 0x2E, 0x0D,
	0x4E, 0xD5, 0x52, 0xD5, 0x0B, 0xFD, 0x88, 0xCE, 0x21, 0x40, 0x3E, 0xE8, 0x9B, 0x81, 0xC6,
	0xFE, 0x30, 0x92, 0x2D, 0xBD, 0xE3, 0x8C, 0x78, 0x00, 0xA3, 0xDB, 0x68, 0xF9, 0xDE, 0xC7,
	0xA4, 0x0D, 0x5E, 0x24, 0x95, 0xF6, 0xC5, 0xF1, 0x02, 0xB5, 0x59, 0xB6, 0xB1, 0xC4, 0xB8,
	0xAF, 0x5D, 0x25, 0x4F, 0xC4, 0x3F, 0xED, 0xF1, 0xB6, 0x6D, 0xE9, 0xC1, 0x20, 0x9E, 0x6A,
	0xBB, 0x63, 0x7E, 0x0A, 0x88, 0x9D, 0xB5, 0x6A, 0xC5, 0xB4, 0x9A, 0xDF, 0x00, 0xE4, 0x56,
	0xA6, 0xA3, 0xB9, 0x0A, 0xE9, 0xF1, 0x3D, 0x74, 0xDC, 0xF9, 0x88, 0x7C, 0x2F, 0x06, 0x1D,
	0x7E, 0x6C, 0xD8, 0x45, 0x4B, 0x66, 0xA1, 0x7B, 0xDE, 0x11, 0xE2, 0x33, 0xAA, 0x3E, 0x3C,
	0xFB, 0x8F, 0x97, 0x33, 0x7E, 0xD2, 0xFD, 0xF6, 0x7C, 0xA8, 0xB6, 0x1E, 0x23, 0xC1, 0xEB,
	0xBC, 0x9E, 0x70, 0xF8, 0x5E, 0x82, 0xC3, 0xB5, 0x9B, 0x97, 0x61, 0x25, 0xA2, 0x5B, 0x49,
	0xE8, 0xDD, 0xEA, 0x79, 0x62, 0x38, 0x3B, 0x82, 0x8C,
};

#endif /* _LC3_STEREO_IN_H_ */
