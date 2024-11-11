/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LED_AUDIO_DK_H_
#define _LED_AUDIO_DK_H_

/**
 * @brief DK LED unit indexes.
 */
#define LED_APP_RGB	0
#define LED_NET_RGB	1
#define LED_APP_1_BLUE	2
#define LED_APP_2_GREEN 3
#define LED_APP_3_GREEN 4

/**
 * @brief Mapping of LEDs to a status indication.
 */
#define LED_CORE_STATUS	      (LED_APP_RGB)
#define LED_CONNECTION_STATUS (LED_NET_RGB)
#define LED_AUDIO_STATUS      (LED_APP_1_BLUE)
#define LED_DEVICE_TYPE	      (LED_APP_2_GREEN)
#define LED_ERROR	      (LED_APP_3_GREEN)

/**
 *  @brief Color indexes.
 */
#define LED_COLOR_MAP_RED   0
#define LED_COLOR_MAP_GREEN 1
#define LED_COLOR_MAP_BLUE  2
#define LED_COLOR_MAP_MONO  0

/**
 * @brief Color sizes.
 */
#define COLORS_MONO_NUM 1
#define COLORS_RGB_NUM	3
#define COLORS_MAX_NUM	3

/**
 * @brief Maximum number of LED_UNITS. 1 RGB LED = 1 UNIT of 3 LEDs.
 */
#define LED_UNIT_MAX 5

/**
 * @brief Periodically invoked by the timer to blink LED unit led0.
 *
 * @param work		Pointer to the work item.
 */
static inline void led_unit_0_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_RGB);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led1.
 *
 * @param work		Pointer to the work item.
 */
static inline void led_unit_1_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_NET_RGB);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led2.
 *
 * @param work		Pointer to the work item.
 */
static inline void led_unit_2_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_1_BLUE);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led3.
 *
 * @param work		Pointer to the work item.
 */
static inline void led_unit_3_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_2_GREEN);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led4.
 *
 * @param work		Pointer to the work item.
 */
static inline void led_unit_4_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_3_GREEN);
}

/**
 * @brief LED unit labels.
 */
static const char led0[] = "LED_RGB_RED";
static const char led1[] = "LED_RGB_GREEN";
static const char led2[] = "LED_RGB_BLUE";
static const char led3[] = "LED_MONO";

/**
 * @brief Description table for each LED to search for.
 */
const struct led_description led_description_table[] = {{led0, LED_COLOR_MAP_RED},
							{led1, LED_COLOR_MAP_GREEN},
							{led2, LED_COLOR_MAP_BLUE},
							{led3, LED_COLOR_MAP_MONO}};

#endif /* _LED_AUDIO_DK_H_ */
