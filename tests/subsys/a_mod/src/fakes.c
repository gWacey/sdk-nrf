/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>

#include "common.h"
#include "data_fifo.h"
#include "fakes.h"

/* Overload the message buffer pointer with a point to one of the an arrays below */
static int fifo_num;

/* Number of messages used*/

struct test_data_fifo_buffer {
	int msgq_read_pos;
	int msgq_free_pos;
	int slab_blocks_read_pos;
	int slab_blocks_free_pos;

	char buffer[FAKE_FIFO_MSG_QUEUE_SIZE][FAKE_FIFO_MSG_QUEUE_DATA_SIZE];
	size_t buffer_size;
};

/* FIFO "slab" buffers */
static struct test_data_fifo_buffer test_fifo_data_slab[FAKE_FIFO_NUM];

/* FIFO "queue" items */
static struct test_data_fifo_buffer test_fifo_data_queue[FAKE_FIFO_NUM];

/*
 * Stubs are defined here, so that multiple .C files can share them
 * without having linker issues.
 */
DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(int, data_fifo_pointer_first_vacant_get, struct data_fifo *, void **,
		       k_timeout_t);
DEFINE_FAKE_VALUE_FUNC(int, data_fifo_block_lock, struct data_fifo *, void **, size_t);
DEFINE_FAKE_VALUE_FUNC(int, data_fifo_pointer_last_filled_get, struct data_fifo *, void **,
		       size_t *, k_timeout_t);
DEFINE_FAKE_VOID_FUNC2(data_fifo_block_free, struct data_fifo *, void **);
DEFINE_FAKE_VALUE_FUNC(int, data_fifo_num_used_get, struct data_fifo *, uint32_t *, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, data_fifo_empty, struct data_fifo *);
DEFINE_FAKE_VALUE_FUNC(int, data_fifo_init, struct data_fifo *);

/* Custom fakes implementation */
int fake_data_fifo_pointer_first_vacant_get__succeeds(struct data_fifo *data_fifo, void **data,
						      k_timeout_t timeout)
{
	ARG_UNUSED(data_fifo);
	ARG_UNUSED(timeout);
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	*data = &msg->buffer[msg->slab_blocks_free_pos][0];

	msg->slab_blocks_read_pos = msg->slab_blocks_free_pos;
	msg->slab_blocks_free_pos += 1;

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
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	memcpy(&msg->buffer[msg->msgq_free_pos][0], *data, size);
	msg->buffer_size = size;

	msg->msgq_read_pos = msg->msgq_free_pos;
	msg->msgq_free_pos += 1;

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
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	*data = &msg->buffer[msg->msgq_read_pos][0];
	*size = data_fifo->block_size_max;

	msg->msgq_read_pos -= 1;
	msg->msgq_free_pos -= 1;

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
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	msg->slab_blocks_read_pos -= 1;
	msg->slab_blocks_free_pos -= 1;
}

int fake_data_fifo_num_used_get__succeeds(struct data_fifo *data_fifo, uint32_t *alloced_num,
					  uint32_t *locked_num)
{
	ARG_UNUSED(data_fifo);
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	*alloced_num = msg->msgq_free_pos;
	*locked_num = msg->slab_blocks_free_pos;

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
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	msg->msgq_free_pos = 0;
	msg->msgq_read_pos = 0;
	msg->slab_blocks_free_pos = 0;
	msg->slab_blocks_read_pos = 0;

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
	struct test_data_fifo_buffer *msg = (struct test_data_fifo_buffer *)data_fifo;

	data_fifo->msgq_buffer = (char *)&test_fifo_data_queue[fifo_num];
	data_fifo->slab_buffer = (char *)&test_fifo_data_slab[fifo_num];
	data_fifo->elements_max = FAKE_FIFO_MSG_QUEUE_SIZE;
	data_fifo->block_size_max = FAKE_FIFO_MSG_QUEUE_DATA_SIZE;
	data_fifo->initialized = true;

	fifo_num += 1;
	msg->msgq_read_pos = 0;
	msg->msgq_free_pos = 0;
	msg->slab_blocks_read_pos = 0;
	msg->slab_blocks_free_pos = 0;

	return 0;
}

int fake_data_fifo_init__fails(struct data_fifo *data_fifo)
{
	ARG_UNUSED(data_fifo);

	return -EINVAL;
}
