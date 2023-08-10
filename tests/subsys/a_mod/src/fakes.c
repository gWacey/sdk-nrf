/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "fakes.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include "data_fifo.h"
#include "common_test.h"

DEFINE_FFF_GLOBALS;

/* Overload the message buffer pointer with a point to one of the an arrays below */
static int fifo_num;

/* Number of messages used*/
static int msgq_num_used_in;
static int slab_blocks_num_used;

struct test_data_fifo_buffer {
	char buffer[TEST_MOD_MSG_QUEUE_SIZE][TEST_MOD_DATA_SIZE];
};

/* FIFO "slab" buffers */
static struct test_data_fifo_buffer test_fifo_data_slab[TEST_NUM_MODULES];

/* FIFO "queue" items */
static struct test_data_fifo_buffer test_fifo_data_queue[TEST_NUM_MODULES];

/* Fake functions declaration */
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_pointer_first_vacant_get, struct data_fifo *, void **,
			k_timeout_t);
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_block_lock, struct data_fifo *, void **, size_t);
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_pointer_last_filled_get, struct data_fifo *, void **,
			size_t *, k_timeout_t);
DECLARE_FAKE_VOID_FUNC2(data_fifo_block_free, struct data_fifo *, void **);
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_num_used_get, struct data_fifo *, uint32_t *, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_empty, struct data_fifo *);
DECLARE_FAKE_VALUE_FUNC(int, data_fifo_init, struct data_fifo *);

/* Custom fakes implementation */
int fake_data_fifo_pointer_first_vacant_get__succeeds(struct data_fifo *data_fifo, void **data,
						      k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(timeout);

	*data = &(((struct test_data_fifo_buffer *)data_fifo->slab_buffer)
			  ->buffer[slab_blocks_num_used][0]);

	slab_blocks_num_used += 1;

	return 0;
}

int fake_data_fifo_pointer_first_vacant_get__timeout_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(timeout);

	return -ENOMSG;
}

int fake_data_fifo_pointer_first_vacant_get__no_wait_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(timeout);

	return -EAGAIN;
}

int fake_data_fifo_pointer_first_vacant_get__invalid_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(timeout);

	return -EINVAL;
}

int fake_data_fifo_block_lock__succeeds(struct data_fifo *data_fifo, void **data, size_t size)
{
	memcpy(&((struct test_data_fifo_buffer *)data_fifo->msgq_buffer)
			->buffer[msgq_num_used_in][0],
	       *data, size);

	msgq_num_used_in += 1;

	return 0;
}

int fake_data_fifo_block_lock__size_fails(struct data_fifo *data_fifo, void **data, size_t size)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(size);

	return -ENOMEM;
}

int fake_data_fifo_block_lock__size_0_fails(struct data_fifo *data_fifo, void **data, size_t size)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(size);

	return -EINVAL;
}

int fake_data_fifo_block_lock__put_fails(struct data_fifo *data_fifo, void **data, size_t size)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(size);

	return -ESPIPE;
}

int fake_data_fifo_pointer_last_filled_get__succeeds(struct data_fifo *data_fifo, void **data,
						     size_t *size, k_timeout_t timeout)
{
	ARG_UNUSED(timeout);

	*data = &(((struct test_data_fifo_buffer *)data_fifo->slab_buffer)
			  ->buffer[msgq_num_used_in][0]);
	*size = TEST_MOD_DATA_SIZE;

	msgq_num_used_in -= 1;

	return 0;
}

int fake_data_fifo_pointer_last_filled_get_no_wait_fails(struct data_fifo *data_fifo, void **data,
							 size_t *size, k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(size);
	ARG_UNUSED(timeout);

	return -ENOMSG;
}

int fake_data_fifo_pointer_last_filled_get__timeout_fails(struct data_fifo *data_fifo, void **data,
							  size_t *size, k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);
	ARG_UNUSED(size);

	/* Sleep for timeout */
	k_sleep(timeout);

	return -EAGAIN;
}

void fake_data_fifo_block_free__succeeds(struct data_fifo *data_fifo, void **data)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(data);

	slab_blocks_num_used -= 1;
}

int fake_data_fifo_num_used_get__succeeds(struct data_fifo *data_fifo, uint32_t *alloced_num,
					  uint32_t *locked_num)
{
	ARG_UNUSED(data_fifo);

	*alloced_num = msgq_num_used_in;
	*locked_num = slab_blocks_num_used;

	return 0;
}

int fake_data_fifo_num_used_get__fails(struct data_fifo *data_fifo, uint32_t *alloced_num,
				       uint32_t *locked_num)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(alloced_num);
	ARG_UNUSED(locked_num);

	return -EACCES;
}

int fake_data_fifo_empty__succeeds(struct data_fifo *data_fifo)
{
	msgq_num_used_in = 0;
	slab_blocks_num_used = 0;

	return 0;
}

int fake_data_fifo_empty__count_fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -EACCES;
}

int fake_data_fifo_empty__no_wait_fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -ENOMSG;
}

int fake_data_fifo_empty__slab_init_fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -EINVAL;
}

int fake_data_fifo_empty__timeout_fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -EAGAIN;
}

int fake_data_fifo_init__succeeds(struct data_fifo *data_fifo)
{
	data_fifo->msgq_buffer = (char *)&test_fifo_data_queue[fifo_num];
	data_fifo->slab_buffer = (char *)&test_fifo_data_slab[fifo_num];
	data_fifo->elements_max = TEST_MOD_MSG_QUEUE_SIZE;
	data_fifo->block_size_max = TEST_MOD_DATA_SIZE;
	data_fifo->initialized = true;

	fifo_num += 1;
	msgq_num_used_in = 0;
	slab_blocks_num_used = 0;

	return 0;
}

int fake_data_fifo_init__fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -EINVAL;
}
