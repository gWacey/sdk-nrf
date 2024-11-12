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
 * @brief Private pointer to the LED module's device.
 */
extern const struct device *led_app_dev;

/**
 * @brief LED description structure.
 */
struct led_description {
	/* LED label. */
	const char *label;
	/* Mapping of LED to GPIO specification. */
	uint8_t mapping;
};

/**
 * @brief Base blinking function.
 */
void led_unit_blink(uint8_t led_unit);

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
