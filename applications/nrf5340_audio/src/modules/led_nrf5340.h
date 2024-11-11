/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _LED_NRF5340_H_
#define _LED_NRF5340_H_

#include <stdint.h>

/**
 * @brief Definition of an unused LED.
 */
#define LED_NOT_ASSIGNED 0xFF

/**
 * @brief RGB color mask settings.
 */
enum led_color {
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
 * @brief Structure to define an LED units features.
 */
struct led_unit_info {
	/* LED's label. */
	const char *label;
	/* Index of the LED on the controller. */
	uint32_t index;
	/* Number of colors per LED. */
	uint8_t num_colors;
	/* Mapping of the LED colors. */
	const uint8_t *color_mapping;
};

/**
 * @brief Structure to define an LED units current control information.
 */
struct led_unit_ctrl {
	/* Flag to indicate if the unit is on or off. */
	bool on;
	/* Flag to indicate if the unit is blinking. */
	bool blink;
	/* Color of the unit. */
	enum led_color color;
	/* Work structure. */
	struct k_work *work;
	/* Pointer to a timer. */
	struct k_timer led_timer;
};

/**
 * @brief Structure to define an LED units current configuration.
 */
struct led_unit_cfg {
	/* Number of LEDs in the unit. */
	size_t leds_num;
	/* LED's unit features information. */
	struct led_unit_info info;
	/* LED's unit current control information. */
	struct led_unit_ctrl ctrl;
	/* LED's unit GPIO specifications. */
	const struct gpio_dt_spec *color[COLORS_MAX_NUM];
};

/**
 * @brief Structure to define the LED controller's configuration.
 */
struct led_config {
	/* Number of LEDs in DT. */
	size_t leds_num;
	/* LED label from DT. */
	const char *led_labels;
	/* GPIO specification. */
	const struct gpio_dt_spec *led_gpios;
	/* Number of LED units. */
	size_t led_units_num;
	/* LED unit configurations. */
	struct led_unit_cfg led_units[LED_UNIT_NUM];
};

/**
 * @brief Callback API for writing a strip of LED channels.
 */
typedef void (*led_blink_work_handler)(struct k_work *work);

/**
 * @brief LED description structure.
 */
struct led_description {
	/* LED label. */
	char *label;
	/* Mapping of LED to GPIO specification. */
	uint8_t mapping;
};

/**
 * @brief Structure to define LED unit blink function table.
 */
struct led_unit_blink_work_handler {
	led_blink_work_handler blink_work;
}

/**
 * @brief Base blinking function.
 */
void led_unit_blink(uint8_t led_unit)

/**
 * @brief Select LED configuration for the given board.
 */
#if CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP || CONFIG_BOARD_NRF5340_AUDIO_DK_NRF5340_CPUAPP_NS
#include "led_audio_dk.h"
#elif CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP || CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP_NS
#include "led_dk.h"
#else
#error "No or incorrect board selected."
#endif

#endif /* _LED_NRF5340_H_ */
