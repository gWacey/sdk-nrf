/*
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _DUMMY_H_
#define _DUMMY_H_

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
extern const struct amod_description *dummy_description;

/**
 * @brief The module configuration structure.
 *
 */
struct dummy_configuration {
	/*! The rate */
	uint32_t rate;

	/*! the depth */
	uint32_t depth;

	/*! A string */
	char *some_text;
};

#endif /* _DUMMY_H_ */
