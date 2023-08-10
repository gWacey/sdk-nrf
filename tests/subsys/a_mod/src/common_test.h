/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COMMON_TEST_H_
#define _COMMON_TEST_H_

struct mod_config {
	int test_int1;
	int test_int2;
	int test_int3;
	int test_int4;
};

struct mod_context {
	char *test_string;
	uint32_t test_uint32;

	struct mod_config config;
};

#define TEST_NUM_MODULES	(4)
#define TEST_MOD_STACK_SIZE	(1024)
#define TEST_MOD_MSG_QUEUE_SIZE (4)
#define TEST_MOD_DATA_SIZE	(40)

#endif /* _COMMON_TEST_H_ */
