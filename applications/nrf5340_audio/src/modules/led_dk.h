/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LED_DK_H_
#define _LED_DK_H_

/**
 * @brief DK LED indexes.
 */
#define LED_APP_1_GREEN	   0
#define LED_APP_2_GREEN	   1
#define LED_APP_3_GREEN	   2
#define LED_APP_4_GREEN	   3
#define LED_COLOR_MAP_MONO 0

/**
 * @brief Mapping of LEDs to a status indication.
 */
#define LED_CORE_STATUS	      (LED_APP_2_GREEN)
#define LED_CONNECTION_STATUS (LED_APP_1_GREEN)
#define LED_AUDIO_STATUS      (LED_APP_3_GREEN)
#define LED_DEVICE_TYPE	      (LED_NOT_ASSIGNED)
#define LED_ERROR	      (LED_APP_4_GREEN)

/**
 * @brief Color indexes.
 */
#define COLORS_MONO_NUM 4

/**
 * @brief Color sizes.
 */
#define COLORS_MAX_NUM 1

/**
 * @brief Maximum number of LED_UNITS. 1 RGB LED = 1 UNIT of 3 LEDs.
 */
#define LED_UNIT_MAX 4

/**
 * @brief Periodically invoked by the timer to blink LED unit led0.
 *
 * @param work		Pointer to the work item.
 */
void led_unit_0_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_1_GREEN);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led1.
 *
 * @param work		Pointer to the work item.
 */
void led_unit_1_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_2_GREEN);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led2.
 *
 * @param work		Pointer to the work item.
 */
void led_unit_2_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_3_GREEN);
}

/**
 * @brief Periodically invoked by the timer to blink LED unit led3.
 *
 * @param work		Pointer to the work item.
 */
void led_unit_3_blink_hdl(struct k_work *work)
{
	led_unit_blink(LED_APP_4_GREEN);
}

/**
 * @brief Table of LED unit blink work functions.
 */
k_work_handler_t led_unit_blink_work_handler[] = {&led_unit_0_blink_hdl, &led_unit_1_blink_hdl,
						  &led_unit_2_blink_hdl, &led_unit_3_blink_hdl};

/**
 * @brief LED labels.
 */
static const char led0[] = "LED 0";
static const char led1[] = "LED 1";
static const char led2[] = "LED 2";
static const char led3[] = "LED 3";

/**
 * @brief Description table for each LED to search for.
 */
const struct led_description led_description_table[] = {{led0, LED_COLOR_MAP_MONO},
							{led1, LED_COLOR_MAP_MONO},
							{led2, LED_COLOR_MAP_MONO},
							{led3, LED_COLOR_MAP_MONO}};

#endif /* _LED_DK_H_ */
