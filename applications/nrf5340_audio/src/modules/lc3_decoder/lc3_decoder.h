/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX - License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _LC3_DECODER_H_
#define _LC3_DECODER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "amod_api.h"
#include "amod_api_private.h"
#include "aobj_api.h"

/**
 * @brief Private pointer to the module's parameters.
 *
 */
extern amod_parameters lc3_dec_parameters;

/**
 * @brief The module configuration structure.
 *
 */
struct lc3_decoder_configuration {
	/*! Sample rate for the decoder instance */
	uint32_t sample_rate;

	/*! Bit depth for this decoder instance */
	uint32_t bit_depth;

	/*! Frame duration for this decoder instance */
	uint32_t duration_us;

	/*! Number of channels for this decoder instance */
	uint32_t number_channels;

	/*! Channel map for this decoder instance */
	uint32_t channel_map;
};

#endif _LC3_DECODER_H_
