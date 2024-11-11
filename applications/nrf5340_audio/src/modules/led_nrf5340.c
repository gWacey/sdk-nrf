/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "led_nrf5340.h"

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led, CONFIG_MODULE_LED_LOG_LEVEL);

/**
 * @brief LED unint's get information callback.
 *
 * @param dev[in]			Pointer to the LED device.
 * @param led_unit[in]		The LED unit index.
 * @param info_out[out]		Pointer to the LED unit's information.
 *
 * @return 0 if successful, error otherwise.
 */
static int led_state_set(const struct device *dev, uint32_t led, bool on)
{
	int ret;
	const struct led_config *config;
	struct led_unit_cfg *led_units;
	const struct led_unit_info *info;
	struct led_unit_ctrl *ctrl;

	if (dev == NULL || dev->config == NULL) {
		return -EINVAL;
	}

	config = (const struct led_config *)dev->config;

	if (led >= config->led_units_num) {
		return -EINVAL;
	}

	led_units = config->led_units;

	info = &led_units[led].info;
	ctrl = &led_units[led].ctrl;

	/* Test the on/off times and if set turn back on blinking */
	/* If the on/off times are 0 then turn just the LED on */

	if (info->num_colors == COLORS_RGB_NUM) {
		for (uint8_t i = 0; i < info->num_colors; i++) {
			if (ctrl->color & BIT(i)) {
				ret = gpio_pin_set_dt(led_units[led].color[i], on);
				if (ret) {
					return ret;
				}
			} else {
				ret = gpio_pin_set_dt(led_units[led].color[i], false);
				if (ret) {
					return ret;
				}
			}
		}
	} else if (info->num_colors == COLORS_MONO_NUM) {
		ret = gpio_pin_set_dt(led_units[led].color[0], on);
		if (ret) {
			return ret;
		}
	} else {
		return -EINVAL;
	}

	ctrl->on = on;

	return 0;
}

/**
 * @brief LED unint's get information callback.
 *
 * @param dev[in]			Pointer to the LED device.
 * @param led[in]			The LED index.
 * @param info_out[out]		Pointer to the LED unit's information.
 *
 * @return 0 if successful, error otherwise.
 */
static int led_get_info_cb(const struct device *dev, uint32_t led,
			   const struct led_unit_info **info_out)
{
	const struct led_config *config;

	if (dev == NULL || dev->config == NULL) {
		return -EINVAL;
	}

	*config = dev->config;

	if (led_unit < config->led_units_num) {
		*info_out = &config->led_units[led_unit].info;
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief LED unint's on callback.
 *
 * @param dev[in]		Pointer to the LED device.
 * @param led[in]		The LED index to turn on.
 *
 * @return 0 if successful, error otherwise.
 */
static int led_on_cb(const struct device *dev, uint32_t led)
{
	return led_state_set(dev, led, true);
}

/**
 * @brief LED unint's off callback.
 *
 * @param dev[in]		Pointer to the LED device.
 * @param led[in]		The LED index to turn off.
 *
 * @return 0 if successful, error otherwise.
 */
static int led_off_cb(const struct device *dev, uint32_t led)
{
	int ret;
	const struct led_config *config;

	ret = led_state_set(dev, led, false);
	if (ret) {
		return ret;
	}

	config = (const struct led_config *)dev->config;

	if (config->led_units->ctrl.on_time_ms || config->led_units->ctrl.off_time_ms) {
		/* stop the timer config->led_units->ctrl.led_timer */
		config->led_units->ctrl.on_time_ms = 0;
		config->led_units->ctrl.off_time_ms = 0;
	}
}

/**
 * @brief LED unint's set color callback.
 *
 * @param dev[in]		Pointer to the LED device.
 * @param led[in]		The LED index to set color.
 * @param num_colors	Number of colors in the array.
 * @param color			Array of colors. It must be ordered following the color
 *						mapping of the LED controller. See the color_mapping
 *						member in struct led_info.
 *
 * @return 0 if successful, error otherwise.
 */
static int led_set_color_cb(const struct device *dev, uint32_t led, uint8_t num_colors,
			    const uint8_t *color)
{
	int ret;
	const struct led_config *config;

	if (dev == NULL || dev->config == NULL || color == NULL) {
		return -EINVAL;
	}

	*config = dev->config;

	if (led >= config->led_units_num || config->led_units[led].info.num_colors != num_colors) {
		return -EINVAL;
	}

	config->led_units[led].ctrl.color = color;

	ret = led_state_set(dev, led, config->led_units[led].ctrl.on);
	if (ret) {
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief LED unit's blink callback.
 *
 * @param dev			Pointer to the LED device.
 * @param led			LED index to blink.
 * @param delay_on		Time period (in milliseconds) an LED should be ON.
 * @param delay_off		Unused.
 *
 * @return 0 on success, error otherwise.
 */
static int led_blink_cb(const struct device *dev, uint32_t led, uint32_t delay_on,
			uint32_t delay_off)
{
	ARG_UNUSED(delay_off);

	int ret;
	const struct led_config *config;

	if (dev == NULL || dev->config == NULL) {
		return -EINVAL;
	}

	*config = dev->config;

	/* For the LED unit: */
	/* If delay_on is 0, turn off and stop timer. */
	/* If delay_on set, turn on and start timer. */
	/* delay_off is not used. */

	if (delay_on) {
		ret = led_state_set(dev, led, true);
		if (ret) {
			return ret;
		}

		config = dev->config;

		k_timer_start(&config->led_units->ctrl.led_timer, K_MSEC(delay_on),
			      K_MSEC(delay_on));
	} else {
		k_timer_stop(&config->led_units->ctrl.led_timer);

		ret = led_state_set(dev, led, false);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Search for the given LED.
 *
 * @param led_units		Pointer to the LED unit to configure.
 * @param led_unit		LED unit index.
 * @param led			LED index to search for.
 *
 * @return 0 on success, error otherwise.
 */
static int led_device_search(struct led_unit_cfg *led_units, uint32_t led_unit, uint8_t led)
{
	int ret;
	uint8_t descript_idx;

	for (descript_idx = 0; descript_idx < ARRAY_SIZE(led_description); descript_idx++) {
		struct led_description *led_desc = led_description_table[descript_idx];

		if (strstr(config->led_labels[led], led_desc->label)) {

			if (!device_is_ready(config->led_gpios[led].port)) {
				LOG_ERR("LED %d GPIO controller not ready", led);
				return -ENODEV;
			}

			ret = gpio_pin_configure_dt(led_units[led_unit].color[led_desc->mapping],
						    GPIO_OUTPUT_INACTIVE);
			if (ret) {
				LOG_ERR("GPIO configure failed for LED %d", led);
				return ret;
			}

			if (led_unit_blink_work_handler[led_unit] != NULL) {
				k_work_init(&config->led_units->ctrl.work,
					    led_unit_blink_work_handler[led_unit]);
				k_timer_init(&config->led_units->ctrl.led_timer, led_timer_expire,
					     NULL);
				k_timer_user_data_set(&config->led_units->ctrl.led_timer, led_unit);
			}

			led_units[led_unit].leds_num++;

			return 0;
		}
	}

	LOG_ERR("No identifier for LED %d LED unit %d", i, led_unit);
	return -ENODEV;
}

/*
 * @brief Parses the device tree for LED settings.
 *
 * @param dev			Pointer to the LED device.
 *
 * @return 0 on success, error otherwise.
 */
static int led_device_tree_parse(const struct device *dev)
{
	int ret;
	const struct led_config *config = dev->config;

	for (uint8_t i = 0; i < config->leds_num; i++) {
		char *end_ptr = NULL;
		uint32_t led_unit = strtoul(led_labels[i], &end_ptr, BASE_10);

		if (led_labels[i] == end_ptr) {
			LOG_ERR("No match for led unit. The dts is likely not properly formatted");
			return -ENXIO;
		}

		ret = led_device_search(config->led_units, led_unit, i);
		if (ret) {
			LOG_ERR("Search for LED %d LED unit %d failed", i, led_unit);
			return -ENODEV;
		}
	}

	return 0;
}

/**
 * @brief Periodically invoked by the timer to blink LEDs.
 *
 * @param led_unit		LED unit index to blink.
 */
void led_unit_blink(uint8_t led_unit)
{
	int ret;
	const struct device *dev = DEVICE_DT_GET_ONE(gpio_leds);
	const struct led_config *config;

	if (config->led_units[i].ctrl.on) {
		ret = led_state_set(dev, i, false);
		ERR_CHK(ret);
	} else {
		ret = led_state_set(dev, i, true);
		ERR_CHK(ret);
	}
}

/**
 * @brief Submit a k_work on timer expiry.
 *
 * @param led_timer		Pointer to the timer that expired.
 */
static void led_timer_expire(struct k_timer *led_timer)
{
	const struct device *dev = DEVICE_DT_GET_ONE(gpio_leds);
	const struct led_config *config = (const struct led_config *)dev->config;
	uint32_t led_unit = (uint32_t)k_timer_user_data_get(led_timer);

	k_work_submit(&config->led_units[led_unit].ctrl.work);
}

/*
 * @brief Initialise the LED system.
 *
 * @param dev			Pointer to the LED device.
 *
 * @return 0 on success, error otherwise.
 */
static int led_init(const struct device *dev)
{
	int ret;

	__ASSERT(ARRAY_SIZE(ARRAY_SIZE(gpio_dt_spec_0)) != 0, "No LEDs found in dts");

	leds_num = ARRAY_SIZE(led_gpios);

	ret = led_device_tree_parse(dev);
	if (ret) {
		return ret;
	}

	return 0;
}

/**
 * @brief Table of callbacks for the LEDs.
 */
static const struct led_driver_api led_api_cbs = {
	.on = led_on_cb,
	.off = led_off_cb,
	.blink = led_blink_cb,
	.set_color = led_set_color_cb,
};

#define LED_GPIO_DEVICE(i)                                                                         \
	static const char *const led_labels_##i[] = {                                              \
		DT_FOREACH_CHILD(DT_PATH(led_gpios), DT_LABEL_AND_COMMA)};                         \
                                                                                                   \
	static const struct gpio_dt_spec gpio_leds[] = {                                           \
		DT_FOREACH_CHILD(DT_PATH(led_gpios), GPIO_DT_SPEC_GET_AND_COMMA)};                 \
                                                                                                   \
	static const struct led_gpio_config led_gpio_config_##i = {                                \
		.leds_num = ARRAY_SIZE(gpio_dt_spec_##i),                                          \
		.led_labels = led_labels_##i[],                                                    \
		.led = gpio_dt_spec_##i,                                                           \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(i, &led_init, NULL, NULL, &led_gpio_config_##i, POST_KERNEL,         \
			      CONFIG_LED_INIT_PRIORITY, &led_api_cbs);

DT_INST_FOREACH_STATUS_OKAY(LED_GPIO_DEVICE)
