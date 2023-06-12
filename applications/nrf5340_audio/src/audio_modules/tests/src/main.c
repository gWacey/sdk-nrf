/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>
#include "amod_api.h"

ZTEST(suite_a_mod, test_a_mod_init_fail)
{
	// int ret;

	// ret = amod_open(NULL, NULL, NULL, NULL, NULL);

	ztest_test_pass();
}

ZTEST_SUITE(suite_a_mod, NULL, NULL, NULL, NULL, NULL);
