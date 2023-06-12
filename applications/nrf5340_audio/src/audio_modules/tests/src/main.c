/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <errno.h>

ZTEST(suite_a_mod, test_a_mod_init)
{
	ztest_test_pass();
}

ZTEST_SUITE(suite_a_mod, NULL, NULL, NULL, NULL, NULL);
