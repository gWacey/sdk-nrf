/* Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <led_ctrl.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include <zephyr/dt-bindings/led/led.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(led_control, 4); /* CONFIG_LED_CONTROLLER_LOG_LEVEL); */

/**
 * @brief Number of LEDs to for a single RGB unit.
 */
#define LED_RGB_UNIT_NUM (3)

/**
 * @brief LED color bit masks
 */
#define LED_COLOR_RED_MASK   1
#define LED_COLOR_GREEN_MASK 2
#define LED_COLOR_BLUE_MASK  4

/**
 * @brief The LED device pointer.
 */
const struct device *led_device = DEVICE_DT_GET_ONE(gpio_leds);

/**
 * @brief Switch the LED on.
 *
 * @param led		LED to switch on.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_on(uint32_t led)
{
	int ret;

	ret = led_on(led_device, led);
	if (ret) {
		LOG_WRN("RGB unit %d failed to switch on", led);
		return ret;
	}

	return 0;
}

/**
 * @brief Switch the LED off.
 *
 * @param led		LED to switch off.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_off(uint32_t led)
{
	int ret;

	ret = led_off(led_device, led);
	if (ret) {
		LOG_WRN("RGB unit %d failed to switch off", led);
		return ret;
	}

	return 0;
}

/**
 * @brief Set the color of an RGB LED.
 *
 * @param led		LED to change color.
 * @param color		Color to set the RGB LED unit to.
 *
 * @return 0 on success, error otherwise.
 */
int led_ctrl_set_color(uint32_t led, enum led_rgb_color color)
{
	int ret;
	uint8_t color_map[LED_RGB_UNIT_NUM];

	if (color < LED_COLOR_NUM) {
		color_map[0] = color & LED_COLOR_RED_MASK;
		color_map[1] = color & LED_COLOR_GREEN_MASK;
		color_map[2] = color & LED_COLOR_BLUE_MASK;
	} else {
		LOG_WRN("RGB unit can not display this color (%d)", color);
		return -EINVAL;
	}

	ret = led_set_color(led_device, led, LED_RGB_UNIT_NUM, color_map);
	if (ret) {
		LOG_WRN("RGB unit %d failed to set the color", led);
		return ret;
	}

	return 0;
}

int led_ctrl_blink(uint32_t led, uint32_t delay_ms_on, uint32_t delay_ms_off)
{
#ifdef CONFIG_GPIO_BLINK
	int ret;

	ret = led_blink(led_device, led, delay_ms_on, delay_ms_off);
	if (ret) {
		LOG_WRN("LED %d failed to start blinking", led);
		return ret;
	}
#endif

	return 0;
}
