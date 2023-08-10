/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _FAKES_H_
#define _FAKES_H_

#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include "data_fifo.h"
#include "common_test.h"

/* Custom fakes implementation */
int fake_data_fifo_pointer_first_vacant_get__succeeds(struct data_fifo *data_fifo, void **data,
						      k_timeout_t timeout);
int fake_data_fifo_pointer_first_vacant_get__timeout_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout);
int fake_data_fifo_pointer_first_vacant_get__no_wait_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout);
int fake_data_fifo_pointer_first_vacant_get__invalid_fails(struct data_fifo *data_fifo, void **data,
							   k_timeout_t timeout);
int fake_data_fifo_block_lock__succeeds(struct data_fifo *data_fifo, void **data, size_t size);
int fake_data_fifo_block_lock__size_fials(struct data_fifo *data_fifo, void **data, size_t size);
int fake_data_fifo_block_lock__size_0_fials(struct data_fifo *data_fifo, void **data, size_t size);
int fake_data_fifo_block_lock__put_fials(struct data_fifo *data_fifo, void **data, size_t size);
int fake_data_fifo_pointer_last_filled_get__succeeds(struct data_fifo *data_fifo, void **data,
						     size_t *size, k_timeout_t timeout);
int fake_data_fifo_pointer_last_filled_get_no_wait_fails(struct data_fifo *data_fifo, void **data,
							 size_t *size, k_timeout_t timeout);
int fake_data_fifo_pointer_last_filled_get__timeout_fails(struct data_fifo *data_fifo, void **data,
							  size_t *size, k_timeout_t timeout);
void fake_data_fifo_block_free__succeeds(struct data_fifo *data_fifo, void **data);
int fake_data_fifo_num_used_get__succeeds(struct data_fifo *data_fifo, uint32_t *alloced_num,
					  uint32_t *locked_num);
int fake_data_fifo_num_used_get__fails(struct data_fifo *data_fifo, uint32_t *alloced_num,
				       uint32_t *locked_num);
int fake_data_fifo_empty__succeeds(struct data_fifo *data_fifo);
int fake_data_fifo_empty__count_fails(struct data_fifo *data_fifo);
int fake_data_fifo_empty__no_wait_fails(struct data_fifo *data_fifo);
int fake_data_fifo_empty__slab_init_fails(struct data_fifo *data_fifo);
int fake_data_fifo_empty__timeout_fails(struct data_fifo *data_fifo);
int fake_data_fifo_init__succeeds(struct data_fifo *data_fifo);
int fake_data_fifo_init__fails(struct data_fifo *data_fifo);

#endif /* _FAKES_H_ */
