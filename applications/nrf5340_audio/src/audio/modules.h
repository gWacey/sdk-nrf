/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _MODULES_H_
#define _MODULES_H_

#define CONFIG_MODULES_NUM (2)
#define CONFIG_LC3_DECODER_NUM (2)

enum module_id { MODULE_ID_1 = 0, MODULE_ID_2, MODULES_ID_NUM };
enum lc3_decoder_id { LC3_DECODER_ID_1 = 0, LC3_DECODER_ID_2, LC3_DECODER_ID_NUM };

#endif /* _MODULES_H_ */
