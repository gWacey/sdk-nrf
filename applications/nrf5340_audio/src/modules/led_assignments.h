/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LED_ASSIGNMENTS_H_
#define _LED_ASSIGNMENTS_H_

#include <zephyr/devicetree.h>
#include <stdint.h>
#include <led_ctrl.h>

/**
 * @brief Mapping of LEDs on board to indication.
 */
#if DT_NODE_EXISTS(DT_ALIAS(connection_status))
#define LED_CONNECTION_STATUS DT_NODE_CHILD_IDX(DT_ALIAS(connection_status))
#else
#define LED_CONNECTION_STATUS LED_NOT_ASSIGNED
#endif

#if DT_NODE_EXISTS(DT_ALIAS(core_status))
#define LED_CORE_STATUS DT_NODE_CHILD_IDX(DT_ALIAS(core_status))
#else
#define LED_CORE_STATUS LED_NOT_ASSIGNED
#endif

#if DT_NODE_EXISTS(DT_ALIAS(device_type))
#define LED_DEVICE_TYPE DT_NODE_CHILD_IDX(DT_ALIAS(device_type))
#else
#define LED_DEVICE_TYPE LED_NOT_ASSIGNED
#endif

#if DT_NODE_EXISTS(DT_ALIAS(audio_status))
#define LED_AUDIO_STATUS DT_NODE_CHILD_IDX(DT_ALIAS(audio_status))
#else
#define LED_AUDIO_STATUS LED_NOT_ASSIGNED
#endif

#if DT_NODE_EXISTS(DT_ALIAS(sync_status))
#define LED_SYNC_STATUS DT_NODE_CHILD_IDX(DT_ALIAS(sync_status))
#else
#define LED_SYNC_STATUS LED_NOT_ASSIGNED
#endif

/**
 * @brief Time in ms an LED should be on when blinking.
 */
#define LED_BLINK_MS_ON 100

/**
 * @brief Time in ms an LED should be off when blinking.
 */
#define LED_BLINK_MS_OFF 100

#endif /* _LED_ASSIGNMENTS_H_ */
