/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include "audio_module.h"

#define FAKE_FIFO_QUEUE_SIZE	      (4)
#define FAKE_FIFO_MSG_QUEUE_SIZE      (FAKE_FIFO_QUEUE_SIZE + 1)
#define FAKE_FIFO_MSG_QUEUE_DATA_SIZE (sizeof(struct audio_module_message))
#define FAKE_FIFO_NUM		      (2)

#define TEST_MOD_THREAD_STACK_SIZE (1024)
#define TEST_MOD_THREAD_PRIORITY   (4)
#define TEST_CONNECTIONS_NUM	   (5)
#define TEST_MODULES_NUM	   (TEST_CONNECTIONS_NUM - 1)
#define TEST_MOD_DATA_SIZE	   (40)

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

#endif /* _COMMON_H_ */
