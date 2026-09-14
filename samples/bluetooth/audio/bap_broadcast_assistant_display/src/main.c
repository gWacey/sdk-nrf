/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Broadcast Assistant sample with a touch UI on a round display.
 *
 * Flow:
 *   0. Boots into an idle clock screen (Nordic logo + time). Tapping it
 *      starts scanning.
 *   1. Scan for Broadcast Sources and show them on screen.
 *   2. User taps a source -> periodic advertising sync to it starts in the
 *      background, and the UI switches to scanning for Broadcast Sinks.
 *      Tapping Exit at any point during 1-3 stops scanning/disconnects and
 *      returns to the clock screen.
 *   3. User taps a sink -> we connect to it, discover its Broadcast Audio
 *      Scan Service, and (once the source's BASE has been received) add the
 *      selected source to it, so the sink starts receiving the broadcast;
 *      it shows up greyed out and stays connected. Sink scanning keeps
 *      running, so more sinks can be picked the same way.
 *   4. Tapping a greyed-out sink removes the source from it over the same
 *      connection (no reconnect), so it stops receiving and goes back to
 *      being selectable. Tapping it again adds the source back. Either way
 *      the sink stays connected the whole time.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_scan_ctrl.h"
#include "ui.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define UI_POLL_PERIOD	   K_MSEC(10)
#define SPLASH_DURATION_MS 2000U

static void handle_row_selected(enum ui_screen screen, uint8_t index);
static void handle_clock_tapped(void);
static void handle_exit(void);
static void handle_source_found(uint8_t index, const struct ble_broadcast_source *source);
static void handle_sink_found(uint8_t index, const struct ble_broadcast_sink *sink);
static void handle_sink_connected(uint8_t index);
static void handle_sink_disconnected(uint8_t index);
static void handle_source_synced(void);
static void handle_source_added(uint8_t index);
static void handle_source_removed(uint8_t index);
static void handle_op_failed(enum ble_scan_ctrl_op op, uint8_t sink_index, int err);

static const struct ble_scan_ctrl_cb ble_callbacks = {
	.source_found = handle_source_found,
	.sink_found = handle_sink_found,
	.sink_connected = handle_sink_connected,
	.sink_disconnected = handle_sink_disconnected,
	.source_synced = handle_source_synced,
	.source_added = handle_source_added,
	.source_removed = handle_source_removed,
	.op_failed = handle_op_failed,
};

/** Drop all BLE state and restart the whole demo from the source-scan screen. */
static void restart_demo(void)
{
	int ret;

	(void)ble_scan_ctrl_reset();
	ui_show_source_list();

	ret = ble_scan_ctrl_start_source_scan();
	if (ret != 0) {
		LOG_ERR("Failed to start source scan (ret %d)", ret);
		ui_show_status("Scan error", "Could not start source scan");
	}
}

/** True for failures that mean the selected source itself is gone, not just one sink. */
static bool op_is_source_level(enum ble_scan_ctrl_op op)
{
	return op == BLE_SCAN_CTRL_OP_SOURCE_SCAN || op == BLE_SCAN_CTRL_OP_SOURCE_PA_SYNC;
}

static void handle_row_selected(enum ui_screen screen, uint8_t index)
{
	int ret;

	switch (screen) {
	case UI_SCREEN_SOURCE_LIST:
		ret = ble_scan_ctrl_select_source(index);
		if (ret != 0) {
			LOG_ERR("Failed to select source %u (ret %d)", index, ret);
			ui_show_status("Selection failed", "Could not select that source");
			return;
		}

		LOG_INF("Source %u selected; scanning for sinks", index);
		ui_show_sink_list();

		ret = ble_scan_ctrl_start_sink_scan();
		if (ret != 0) {
			LOG_ERR("Failed to start sink scan (ret %d)", ret);
			ui_show_status("Scan error", "Could not start sink scan");
		}
		break;

	case UI_SCREEN_SINK_LIST:
		/* Connects a never-seen sink, or toggles an already-connected one between
		 * receiving and not receiving the source - see ble_scan_ctrl_select_sink().
		 * Every outcome (including failure) arrives later via one of the
		 * ble_callbacks below, keyed by sink index, so there is nothing to do here
		 * beyond starting it and quietly dropping a redundant tap.
		 */
		ret = ble_scan_ctrl_select_sink(index);
		if (ret == -EBUSY) {
			LOG_WRN("Sink %u already has an operation in progress", index);
		} else if (ret != 0) {
			LOG_ERR("Failed to toggle sink %u (ret %d)", index, ret);
		}
		break;

	case UI_SCREEN_CLOCK:
	case UI_SCREEN_STATUS:
	default:
		/* No tappable rows on these screens; nothing to do. */
		break;
	}
}

static void handle_clock_tapped(void)
{
	LOG_INF("Clock tapped; starting scan");
	restart_demo();
}

static void handle_exit(void)
{
	LOG_INF("Exit tapped; returning to clock");
	(void)ble_scan_ctrl_reset();
	ui_show_clock();
}

static void handle_source_found(uint8_t index, const struct ble_broadcast_source *source)
{
	if (source == NULL) {
		return;
	}

	LOG_INF("Source[%u]: %s (id 0x%06x)", index, source->name, source->broadcast_id);
	(void)ui_add_source_row(source->name, source->broadcast_id);
}

static void handle_sink_found(uint8_t index, const struct ble_broadcast_sink *sink)
{
	if (sink == NULL) {
		return;
	}

	LOG_INF("Sink[%u]: %s", index, sink->name);
	(void)ui_add_sink_row(sink->name);
}

static void handle_sink_connected(uint8_t index)
{
	LOG_INF("Sink %u connected and its Broadcast Audio Scan Service was discovered", index);
}

static void handle_sink_disconnected(uint8_t index)
{
	LOG_WRN("Sink %u disconnected unexpectedly", index);
	ui_set_sink_configured(index, false);
}

static void handle_source_synced(void)
{
	LOG_INF("Periodic advertising synced to the selected source");
}

static void handle_source_added(uint8_t index)
{
	LOG_INF("Source added to sink %u; it is now receiving the broadcast", index);
	ui_set_sink_configured(index, true);
}

static void handle_source_removed(uint8_t index)
{
	LOG_INF("Source removed from sink %u; it stopped receiving the broadcast", index);
	ui_set_sink_configured(index, false);
}

/*
 * BLE_SCAN_CTRL_OP_SOURCE_SCAN/SOURCE_PA_SYNC mean the selected source itself is gone
 * (never found, or its periodic advertising sync was lost - source switched off, went
 * out of range, or the controller gave up) - nothing to preserve, so restart from
 * scratch. Every other op is specific to one sink (its own connect/security/BASS/
 * add-source/remove-source step) and does not affect the source or any other sink, so
 * just log it - that sink's row already reflects reality (still, or not yet, greyed
 * out) since a failed add/remove leaves ble_scan_ctrl's own state unchanged.
 */
static void handle_op_failed(enum ble_scan_ctrl_op op, uint8_t sink_index, int err)
{
	if (op_is_source_level(op)) {
		LOG_ERR("Operation %d failed (err %d)", (int)op, err);
		restart_demo();
		return;
	}

	LOG_ERR("Operation %d failed for sink %u (err %d)", (int)op, sink_index, err);
}

/** Keep driving the LVGL loop for at least duration_ms without blocking it. */
static void pump_ui_for(uint32_t duration_ms)
{
	int64_t deadline = k_uptime_get() + duration_ms;

	do {
		ui_process();
		k_sleep(UI_POLL_PERIOD);
	} while (k_uptime_get() < deadline);
}

int main(void)
{
	int ret;

	ret = ui_init(handle_row_selected, handle_clock_tapped, handle_exit);
	if (ret != 0) {
		LOG_ERR("Failed to initialize UI (ret %d)", ret);
		return 0;
	}

	ret = ble_scan_ctrl_init(&ble_callbacks);
	if (ret != 0) {
		LOG_ERR("Failed to initialize Broadcast Assistant control (ret %d)", ret);
		return 0;
	}

	ui_show_splash();
	pump_ui_for(SPLASH_DURATION_MS);

	ui_show_clock();

	while (true) {
		ui_process();
		k_sleep(UI_POLL_PERIOD);
	}

	return 0;
}
