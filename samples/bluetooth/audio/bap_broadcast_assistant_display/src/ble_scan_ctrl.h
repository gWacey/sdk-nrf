/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Bluetooth LE Audio Broadcast Assistant control logic.
 *
 * This module implements the BAP Broadcast Assistant role: it scans for
 * Broadcast Sources (LE Audio broadcasters), scans for Broadcast Sinks
 * (devices implementing a BASS server / Scan Delegator role), connects to
 * selected sinks, and toggles whether each one is configured (via the
 * Broadcast Audio Scan Service) to synchronize to the selected source.
 *
 * Sinks stay connected once picked, for as long as their connection holds up:
 * toggling a sink between receiving and not receiving the source only ever
 * adds or removes it from that sink's Broadcast Audio Scan Service over the
 * existing connection, never reconnects. Several sinks can be connected and
 * independently toggled at the same time, up to @ref BLE_SCAN_CTRL_MAX_SINK_CONN.
 *
 * All public functions may be called from any single application thread
 * (the application must not call them concurrently from multiple threads).
 * All callbacks in @ref ble_scan_ctrl_cb are invoked from Bluetooth stack
 * context (the system workqueue or the Bluetooth RX thread) and must not
 * block.
 */

#ifndef BLE_SCAN_CTRL_H_
#define BLE_SCAN_CTRL_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length, including the terminating NUL, of a cached device name. */
#define BLE_SCAN_CTRL_NAME_LEN 32U

/** Maximum number of distinct sources/sinks tracked for one selected source. */
#define BLE_SCAN_CTRL_MAX_RESULTS 8U

/** Maximum number of sinks that may be connected (and independently toggled) at once. */
#define BLE_SCAN_CTRL_MAX_SINK_CONN 4U

/** Information describing a discovered Broadcast Source (LE Audio broadcaster). */
struct ble_broadcast_source {
	/** Best-effort display name (Bluetooth name or Broadcast Name). Always NUL-terminated. */
	char name[BLE_SCAN_CTRL_NAME_LEN];
	/** 24-bit Broadcast ID advertised by the source. */
	uint32_t broadcast_id;
	/** Advertiser address. */
	bt_addr_le_t addr;
	/** Advertising Set ID of the source's extended advertising set. */
	uint8_t sid;
	/** Periodic advertising interval, or BT_BAP_PA_INTERVAL_UNKNOWN. */
	uint16_t pa_interval;
};

/** Information describing a discovered Broadcast Sink (BASS server / Scan Delegator). */
struct ble_broadcast_sink {
	/** Best-effort display name (Bluetooth name). Always NUL-terminated. */
	char name[BLE_SCAN_CTRL_NAME_LEN];
	/** Connectable advertiser address. */
	bt_addr_le_t addr;
};

/** Identifies which asynchronous operation an error report in @ref ble_scan_ctrl_cb refers to. */
enum ble_scan_ctrl_op {
	BLE_SCAN_CTRL_OP_SOURCE_SCAN,
	BLE_SCAN_CTRL_OP_SINK_SCAN,
	BLE_SCAN_CTRL_OP_SINK_CONNECT,
	BLE_SCAN_CTRL_OP_SINK_SECURITY,
	BLE_SCAN_CTRL_OP_BASS_DISCOVER,
	BLE_SCAN_CTRL_OP_SOURCE_PA_SYNC,
	BLE_SCAN_CTRL_OP_ADD_SOURCE,
	BLE_SCAN_CTRL_OP_REMOVE_SOURCE,
};

/**
 * @brief Application event callbacks.
 *
 * Every member is optional; leave it NULL if the event is not of interest.
 * See the file-level comment for the threading contract. Every callback that
 * takes a sink @p index reports it as the index into the list of sinks
 * reported since the last call to ble_scan_ctrl_start_sink_scan(), same as
 * the index passed in to ble_scan_ctrl_select_sink().
 */
struct ble_scan_ctrl_cb {
	/** A new, previously unseen Broadcast Source was found while source-scanning. */
	void (*source_found)(uint8_t index, const struct ble_broadcast_source *source);
	/** A new, previously unseen Broadcast Sink was found while sink-scanning. */
	void (*sink_found)(uint8_t index, const struct ble_broadcast_sink *sink);
	/** The sink is connected, secured, and its BASS has been discovered. */
	void (*sink_connected)(uint8_t index);
	/** The sink connection was lost (or explicitly closed); it is no longer configured. */
	void (*sink_disconnected)(uint8_t index);
	/** Periodic advertising to the selected source is synced and its BASE was parsed. */
	void (*source_synced)(void);
	/** The source was added to the sink's Broadcast Audio Scan Service; it is receiving. */
	void (*source_added)(uint8_t index);
	/** The source was removed from the sink's Broadcast Audio Scan Service. */
	void (*source_removed)(uint8_t index);
	/**
	 * @brief An asynchronous operation failed.
	 *
	 * @p sink_index is only meaningful for sink-specific ops (anything other than
	 * BLE_SCAN_CTRL_OP_SOURCE_SCAN/SOURCE_PA_SYNC); ignore it otherwise.
	 */
	void (*op_failed)(enum ble_scan_ctrl_op op, uint8_t sink_index, int err);
};

/**
 * @brief Initialize the Broadcast Assistant control module.
 *
 * Enables the Bluetooth stack and registers all internal callbacks. Must be
 * called exactly once, before any other function in this module.
 *
 * @param cb Application callback structure. Must remain valid for the
 *           lifetime of the program. Must not be NULL.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p cb is NULL.
 * @retval Negative errno from bt_enable() on Bluetooth init failure.
 */
int ble_scan_ctrl_init(const struct ble_scan_ctrl_cb *cb);

/**
 * @brief Start scanning for Broadcast Sources.
 *
 * Clears any previously discovered source list. Discovered sources are
 * reported through @ref ble_scan_ctrl_cb.source_found.
 *
 * @retval 0 on success.
 * @retval Negative errno on failure to start scanning.
 */
int ble_scan_ctrl_start_source_scan(void);

/**
 * @brief Start scanning for Broadcast Sinks.
 *
 * Clears any previously discovered sink list. Discovered sinks are reported
 * through @ref ble_scan_ctrl_cb.sink_found. Keeps running (sinks can keep
 * being discovered) alongside any connections made via
 * ble_scan_ctrl_select_sink(), until ble_scan_ctrl_reset() is called.
 *
 * @retval 0 on success.
 * @retval Negative errno on failure to start scanning.
 */
int ble_scan_ctrl_start_sink_scan(void);

/**
 * @brief Select a previously discovered Broadcast Source.
 *
 * Stops scanning and begins periodic advertising sync to the selected
 * source in the background in order to obtain its BASE (subgroup) data.
 *
 * @param index Zero-based index into the list of sources reported since the
 *              last call to ble_scan_ctrl_start_source_scan().
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p index is out of range.
 * @retval Negative errno on failure to create the periodic advertising sync.
 */
int ble_scan_ctrl_select_source(uint8_t index);

/**
 * @brief Toggle a previously discovered Broadcast Sink.
 *
 * Behavior depends on the sink's current state:
 * - Never connected: connects to it (subject to
 *   @ref BLE_SCAN_CTRL_MAX_SINK_CONN), discovers its Broadcast Audio Scan
 *   Service, and once the selected source has been PA-synced, adds it -
 *   the connection is kept afterwards.
 * - Connected and currently receiving the source: removes the source from
 *   its Broadcast Audio Scan Service over the existing connection (no
 *   reconnect), so it stops receiving but stays connected.
 * - Connected and not currently receiving the source: adds the source back
 *   over the existing connection (no reconnect).
 *
 * Every outcome is reported asynchronously through @ref ble_scan_ctrl_cb
 * (sink_connected, source_added, source_removed, or op_failed) rather than
 * through this function's return value, which only reports whether the
 * requested step could be *started*.
 *
 * @param index Zero-based index into the list of sinks reported since the
 *              last call to ble_scan_ctrl_start_sink_scan().
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p index is out of range.
 * @retval -EBUSY if an operation on that sink is already in progress.
 * @retval -ENOMEM if @ref BLE_SCAN_CTRL_MAX_SINK_CONN connections are already in use.
 * @retval Negative errno on failure to start the connection/add/remove step.
 */
int ble_scan_ctrl_select_sink(uint8_t index);

/**
 * @brief Reset all state back to idle.
 *
 * Stops any ongoing scan, disconnects every connected sink, deletes the
 * periodic advertising sync (if any), and clears all discovered/selected
 * data. Safe to call at any time, including when already idle.
 *
 * @retval 0 always.
 */
int ble_scan_ctrl_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SCAN_CTRL_H_ */
