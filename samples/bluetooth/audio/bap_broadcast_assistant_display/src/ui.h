/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Touch UI for the Seeed Studio XIAO Round Display.
 *
 * Renders a scrollable, tappable list of scan results and a status screen.
 * This module owns all LVGL interaction; callers never touch LVGL objects
 * directly. It is safe to call the "mutating" functions below (everything
 * except ui_process()) from a thread other than the one that calls
 * ui_process(), since they take the LVGL lock internally.
 */

#ifndef UI_H_
#define UI_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of rows the list screens can display at once. */
#define UI_MAX_LIST_ITEMS 8U

/** Which screen a row-tap event in @ref ui_row_selected_cb occurred on. */
enum ui_screen {
	UI_SCREEN_CLOCK,
	UI_SCREEN_SOURCE_LIST,
	UI_SCREEN_SINK_LIST,
	UI_SCREEN_STATUS,
};

/**
 * @brief Row-tap callback.
 *
 * Invoked from the thread that calls ui_process(). @p index is guaranteed
 * to be less than the number of rows currently shown on @p screen.
 *
 * @param screen Screen the tap occurred on.
 * @param index  Zero-based row index within that screen's list.
 */
typedef void (*ui_row_selected_cb)(enum ui_screen screen, uint8_t index);

/** Invoked from the thread that calls ui_process() when the clock screen is tapped. */
typedef void (*ui_clock_tapped_cb)(void);

/** Invoked from the thread that calls ui_process() when the Exit button is tapped. */
typedef void (*ui_exit_cb)(void);

/**
 * @brief Initialize the display and build the UI.
 *
 * Must be called once, from the thread that will subsequently call
 * ui_process() in a loop.
 *
 * @param on_row_selected Callback invoked when the user taps a list row.
 *                        Must not be NULL.
 * @param on_clock_tapped Callback invoked when the user taps the clock
 *                        screen (request to start scanning). Must not be
 *                        NULL.
 * @param on_exit         Callback invoked when the user taps the Exit
 *                        button on a list screen (request to return to the
 *                        clock screen). Must not be NULL.
 *
 * @retval 0 on success.
 * @retval -EINVAL if any callback is NULL.
 * @retval -ENODEV if the display device is not ready.
 * @retval Negative errno on other display initialization failures.
 */
int ui_init(ui_row_selected_cb on_row_selected, ui_clock_tapped_cb on_clock_tapped,
	    ui_exit_cb on_exit);

/**
 * @brief Show the startup splash screen.
 *
 * Displays the Nordic Semiconductor logo. The caller is responsible for
 * timing: keep calling ui_process() for as long as the splash should stay
 * on screen, then switch away with ui_show_clock() or similar.
 */
void ui_show_splash(void);

/**
 * @brief Show the idle clock screen.
 *
 * Displays the Nordic Semiconductor logo and the current time (read from
 * the board's RTC, if one is available), and waits for a tap to invoke the
 * on_clock_tapped callback passed to ui_init().
 */
void ui_show_clock(void);

/** Switch to the (initially empty) Broadcast Source list screen. */
void ui_show_source_list(void);

/** Switch to the (initially empty) Broadcast Sink list screen. */
void ui_show_sink_list(void);

/**
 * @brief Append one row to the Broadcast Source list.
 *
 * @param name         Display name; may be empty but must not be NULL.
 * @param broadcast_id 24-bit broadcast ID shown alongside the name.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p name is NULL.
 * @retval -ENOMEM if the list already holds UI_MAX_LIST_ITEMS rows.
 */
int ui_add_source_row(const char *name, uint32_t broadcast_id);

/**
 * @brief Append one row to the Broadcast Sink list, shown normally (not yet receiving).
 *
 * @param name Display name; may be empty but must not be NULL.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p name is NULL.
 * @retval -ENOMEM if the list already holds UI_MAX_LIST_ITEMS rows.
 */
int ui_add_sink_row(const char *name);

/**
 * @brief Update whether a sink row is shown greyed out (currently receiving the source).
 *
 * The row stays tappable either way; tapping it is what toggles this in the first place -
 * this function only reflects that outcome once it is known, it does not cause it.
 *
 * @param index Index this row was reported with (by ui_row_selected_cb, and matching
 *              ble_scan_ctrl's own sink index).
 * @param configured True to show it greyed out, false to show it normally.
 */
void ui_set_sink_configured(uint8_t index, bool configured);

/**
 * @brief Switch to the status screen and show a short message.
 *
 * @param title  Short headline; must not be NULL.
 * @param detail Longer description, or NULL to show no detail text.
 */
void ui_show_status(const char *title, const char *detail);

/** Drive the LVGL timer/render loop. Call periodically from one dedicated thread. */
void ui_process(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H_ */
