/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LED_CTRL_H_
#define _LED_CTRL_H_

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Value to indicate that there is no LED assigned to an indication.
 */
#define LED_NOT_ASSIGNED (-1)

/**
 * @brief RGB color mask settings.
 */
enum led_rgb_color {
	LED_COLOR_OFF,	   /* 000 */
	LED_COLOR_RED,	   /* 001 */
	LED_COLOR_GREEN,   /* 010 */
	LED_COLOR_YELLOW,  /* 011 */
	LED_COLOR_BLUE,	   /* 100 */
	LED_COLOR_MAGENTA, /* 101 */
	LED_COLOR_CYAN,	   /* 110 */
	LED_COLOR_WHITE,   /* 111 */
	LED_COLOR_NUM,
};

/**
 * @brief Switch the LED on.
 *
 * @param led		LED to switch on.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_on(uint32_t led);

/**
 * @brief Switch the LED off.
 *
 * @param led		LED to switch off.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_off(uint32_t led);

/**
 * @brief Set the color of an RGB LED.
 *
 * @param led		LED to switch off.
 * @param color		Color to set the RGB LED unit to.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_set_color(uint32_t led, enum led_rgb_color color);

/**
 * @brief Set LED blinking.
 *
 * @param led			LED to change color.
 * @param delay_ms_on	Period LED is on in ms.
 * @param delay_ms_off	Period LED is off in ms.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_blink(uint32_t led, uint32_t delay_ms_on, uint32_t delay_ms_off);

#endif /* _LED_CTRL_H_ */
