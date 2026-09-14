/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_scan_ctrl.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/assigned_numbers.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble_scan_ctrl, LOG_LEVEL_INF);

/*
 * Threading model
 * ----------------
 * Bluetooth stack callbacks (scan results, connection/security state,
 * BASS discovery/add-source/remove-source results, periodic advertising sync) run on
 * Bluetooth stack context (system workqueue / RX thread). The
 * ble_scan_ctrl_select_*() and ble_scan_ctrl_start_*_scan() functions are
 * called by the application from a different thread (typically the thread
 * driving the UI). All state shared between those two contexts is
 * protected by state_mutex.
 *
 * Rule enforced throughout this file: state_mutex is only ever held around
 * the read/modify of shared state itself. It is always released *before*
 * invoking an application callback (struct ble_scan_ctrl_cb member) or
 * calling into another Bluetooth API, so callbacks can safely call back
 * into this module's public API without deadlocking.
 *
 * Sink connections
 * -----------------
 * Up to BLE_SCAN_CTRL_MAX_SINK_CONN sinks can be connected at once, each
 * tracked by its own struct sink_ctx. A sink's connection is kept for as
 * long as it holds up: toggling whether it receives the selected source
 * only adds or removes it from that sink's own Broadcast Audio Scan
 * Service over the existing connection - it never reconnects. Each ctx's
 * "busy" flag marks that some step of connect/discover/add/remove/learn is
 * in flight for it, and is owned by whichever public API call started that
 * step; only the function that concludes the step (successfully or not)
 * clears it.
 */

#define PA_SYNC_SKIP			  5U
#define PA_SYNC_INTERVAL_TO_TIMEOUT_RATIO 20U

/** Internal scanning mode; determines how scan_recv_cb() interprets adverts. */
enum scan_mode {
	SCAN_MODE_IDLE = 0,
	SCAN_MODE_SOURCE,
	SCAN_MODE_SINK,
};

/** Scratch struct populated while parsing one advertising report. */
struct adv_parse_result {
	char name[BLE_SCAN_CTRL_NAME_LEN];
	char broadcast_name[BLE_SCAN_CTRL_NAME_LEN];
	uint32_t broadcast_id;
	bool has_bass;
	bool has_pacs;
};

/** Per-connection state for one sink that has been (or is being) connected. */
struct sink_ctx {
	bool in_use;
	bt_addr_le_t addr;
	struct bt_conn *conn;
	bool bass_discovered;
	/** Number of BASS receive states this sink reported at discovery time. */
	uint8_t recv_state_count;
	/** True while the selected source is added to this sink's BASS (it is receiving). */
	bool configured;
	bool has_src_id;
	uint8_t src_id;
	/** True while a connect/discover/add/remove/learn step is in flight for this sink. */
	bool busy;
	/** Set just before we disconnect this sink ourselves, so disconnected_cb() knows not
	 * to report it as an unexpected loss.
	 */
	bool intentional_disconnect;
	/** True while reading back receive states to learn/refresh src_id. */
	bool learning_src_id;
	/** True if, once src_id is learned, the source should immediately be removed. */
	bool learn_then_remove;
	uint8_t learn_recv_state_idx;
};

static const struct ble_scan_ctrl_cb *app_cb;

/* --- Shared state; always accessed with state_mutex held. --- */
static struct k_mutex state_mutex;

static enum scan_mode current_scan_mode = SCAN_MODE_IDLE;

static struct ble_broadcast_source found_sources[BLE_SCAN_CTRL_MAX_RESULTS];
static uint8_t found_source_count;

static struct ble_broadcast_sink found_sinks[BLE_SCAN_CTRL_MAX_RESULTS];
static uint8_t found_sink_count;

static struct ble_broadcast_source selected_source;
static bool source_selected;
static bool source_base_ready;

static struct sink_ctx sink_ctxs[BLE_SCAN_CTRL_MAX_SINK_CONN];

static struct bt_le_per_adv_sync *pa_sync;
/* --- End of state protected by state_mutex. --- */

/* The raw BASE buffer has its own mutex: it is written from periodic
 * advertising reports and read back when building the add-source request,
 * both of which always happen on Bluetooth stack context, but keeping it
 * separate avoids holding state_mutex across the (larger) BASE parsing work.
 */
static struct k_mutex base_mutex;
static uint8_t received_base[UINT8_MAX];
static size_t received_base_size;

static struct bt_bap_bass_subgroup add_src_subgroups[CONFIG_BT_BAP_BASS_MAX_SUBGROUPS];

/** Copy at most sizeof(dst) - 1 bytes from a (possibly non-NUL-terminated) AD field into dst. */
static void safe_copy_name(char *dst, size_t dst_size, const uint8_t *src, uint8_t src_len)
{
	size_t copy_len;

	if (dst == NULL || dst_size == 0U) {
		return;
	}

	if (src == NULL || src_len == 0U) {
		dst[0] = '\0';
		return;
	}

	copy_len = MIN((size_t)src_len, dst_size - 1U);
	memcpy(dst, src, copy_len);
	dst[copy_len] = '\0';
}

/** Caller must hold state_mutex. */
static bool addr_in_source_list(const bt_addr_le_t *addr)
{
	if (addr == NULL) {
		return false;
	}

	for (uint8_t i = 0U; i < found_source_count && i < BLE_SCAN_CTRL_MAX_RESULTS; i++) {
		if (bt_addr_le_cmp(&found_sources[i].addr, addr) == 0) {
			return true;
		}
	}

	return false;
}

/** Caller must hold state_mutex. */
static bool addr_in_sink_list(const bt_addr_le_t *addr)
{
	if (addr == NULL) {
		return false;
	}

	for (uint8_t i = 0U; i < found_sink_count && i < BLE_SCAN_CTRL_MAX_RESULTS; i++) {
		if (bt_addr_le_cmp(&found_sinks[i].addr, addr) == 0) {
			return true;
		}
	}

	return false;
}

/** Caller must hold state_mutex. Returns the found_sinks[] index for addr, or -1. */
static int find_sink_list_index(const bt_addr_le_t *addr)
{
	if (addr == NULL) {
		return -1;
	}

	for (uint8_t i = 0U; i < found_sink_count && i < BLE_SCAN_CTRL_MAX_RESULTS; i++) {
		if (bt_addr_le_cmp(&found_sinks[i].addr, addr) == 0) {
			return (int)i;
		}
	}

	return -1;
}

/** Caller must hold state_mutex. */
static struct sink_ctx *find_ctx_by_addr(const bt_addr_le_t *addr)
{
	if (addr == NULL) {
		return NULL;
	}

	for (uint8_t i = 0U; i < BLE_SCAN_CTRL_MAX_SINK_CONN; i++) {
		if (sink_ctxs[i].in_use && bt_addr_le_cmp(&sink_ctxs[i].addr, addr) == 0) {
			return &sink_ctxs[i];
		}
	}

	return NULL;
}

/** Caller must hold state_mutex. */
static struct sink_ctx *find_ctx_by_conn(struct bt_conn *conn)
{
	if (conn == NULL) {
		return NULL;
	}

	for (uint8_t i = 0U; i < BLE_SCAN_CTRL_MAX_SINK_CONN; i++) {
		if (sink_ctxs[i].in_use && sink_ctxs[i].conn == conn) {
			return &sink_ctxs[i];
		}
	}

	return NULL;
}

/** Caller must hold state_mutex. Returns NULL if all connection slots are in use. */
static struct sink_ctx *alloc_ctx(const bt_addr_le_t *addr)
{
	for (uint8_t i = 0U; i < BLE_SCAN_CTRL_MAX_SINK_CONN; i++) {
		if (!sink_ctxs[i].in_use) {
			memset(&sink_ctxs[i], 0, sizeof(sink_ctxs[i]));
			sink_ctxs[i].in_use = true;
			bt_addr_le_copy(&sink_ctxs[i].addr, addr);
			return &sink_ctxs[i];
		}
	}

	return NULL;
}

/** Caller must hold state_mutex. */
static int free_ctx(struct sink_ctx *ctx)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

/* Looks up ctx's current position in found_sinks[] and, if found, invokes fn with it. Must
 * be called without state_mutex held; it takes it internally.
 */
static int notify_indexed(struct sink_ctx *ctx, void (*fn)(uint8_t))
{
	bt_addr_le_t addr_snapshot;
	int idx;

	if (ctx == NULL || fn == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	addr_snapshot = ctx->addr;
	idx = find_sink_list_index(&addr_snapshot);
	k_mutex_unlock(&state_mutex);

	if (idx >= 0) {
		fn((uint8_t)idx);
	}

	return 0;
}

/** For failures with no specific sink (source-level or scan-level); reports sink_index 0. */
static void notify_op_failed(enum ble_scan_ctrl_op op, int err)
{
	if (app_cb != NULL && app_cb->op_failed != NULL) {
		app_cb->op_failed(op, 0U, err);
	}
}

/* Same as notify_op_failed(), but resolves ctx's sink-list index for the report. Must be
 * called without state_mutex held; it takes it internally.
 */
static void notify_op_failed_ctx(struct sink_ctx *ctx, enum ble_scan_ctrl_op op, int err)
{
	bt_addr_le_t addr_snapshot;
	int idx = -1;

	if (ctx != NULL) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		addr_snapshot = ctx->addr;
		idx = find_sink_list_index(&addr_snapshot);
		k_mutex_unlock(&state_mutex);
	}

	if (app_cb != NULL && app_cb->op_failed != NULL) {
		app_cb->op_failed(op, idx >= 0 ? (uint8_t)idx : 0U, err);
	}
}

/* Callback for bt_data_parse(): pulls name / broadcast-ID / BASS+PACS presence out of one AD
 * field.
 */
static bool device_found(struct bt_data *data, void *user_data)
{
	struct adv_parse_result *result = (struct adv_parse_result *)user_data;

	if (data == NULL || result == NULL) {
		return false;
	}

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		safe_copy_name(result->name, sizeof(result->name), data->data, data->data_len);
		return true;
	case BT_DATA_BROADCAST_NAME:
		safe_copy_name(result->broadcast_name, sizeof(result->broadcast_name), data->data,
			       data->data_len);
		return true;
	case BT_DATA_SVC_DATA16: {
		struct bt_uuid_16 adv_uuid;

		if (data->data == NULL ||
		    data->data_len < BT_UUID_SIZE_16 + BT_AUDIO_BROADCAST_ID_SIZE) {
			return true;
		}

		if (!bt_uuid_create(&adv_uuid.uuid, data->data, BT_UUID_SIZE_16)) {
			return true;
		}

		if (bt_uuid_cmp(&adv_uuid.uuid, BT_UUID_BROADCAST_AUDIO) != 0) {
			return true;
		}

		result->broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);
		return true;
	}
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data == NULL || (data->data_len % sizeof(uint16_t)) != 0U) {
			return true;
		}

		for (size_t i = 0U; i + sizeof(uint16_t) <= data->data_len; i += sizeof(uint16_t)) {
			const struct bt_uuid *uuid;
			uint16_t u16;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));

			if (bt_uuid_cmp(uuid, BT_UUID_BASS) == 0) {
				result->has_bass = true;
			} else if (bt_uuid_cmp(uuid, BT_UUID_PACS) == 0) {
				result->has_pacs = true;
			}
		}
		return true;
	default:
		return true;
	}
}

/* Callback for bt_data_parse(): extracts the BASE from a periodic advertising report, if
 * present.
 */
static bool base_store(struct bt_data *data, void *user_data)
{
	const struct bt_bap_base *base;
	int base_size;
	int subgroup_count;
	int err;

	ARG_UNUSED(user_data);

	if (data == NULL) {
		return true;
	}

	base = bt_bap_base_get_base_from_ad(data);
	if (base == NULL) {
		/* Not a BASE AD structure; keep looking at other AD fields. */
		return true;
	}

	subgroup_count = bt_bap_base_get_subgroup_count(base);
	if (subgroup_count < 0 || subgroup_count > CONFIG_BT_BAP_BASS_MAX_SUBGROUPS) {
		LOG_WRN("Ignoring BASE with invalid subgroup count: %d", subgroup_count);
		return true;
	}

	base_size = bt_bap_base_get_size(base);
	if (base_size < 0 || (size_t)base_size > sizeof(received_base)) {
		LOG_WRN("Ignoring BASE with invalid size: %d", base_size);
		return true;
	}

	err = k_mutex_lock(&base_mutex, K_MSEC(100));
	if (err != 0) {
		/* Could not get the lock in time; try again on the next report. */
		return false;
	}

	memcpy(received_base, base, (size_t)base_size);
	received_base_size = (size_t)base_size;

	(void)k_mutex_unlock(&base_mutex);

	/* Stop parsing: we only need the first valid BASE. */
	return false;
}

static bool add_src_bis_cb(const struct bt_bap_base_subgroup_bis *bis, void *user_data)
{
	struct bt_bap_bass_subgroup *subgroup_param = (struct bt_bap_bass_subgroup *)user_data;

	if (bis == NULL || subgroup_param == NULL) {
		return false;
	}

	subgroup_param->bis_sync |= BT_ISO_BIS_INDEX_BIT(bis->index);

	return true;
}

static bool add_src_subgroup_cb(const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	struct bt_bap_broadcast_assistant_add_src_param *param =
		(struct bt_bap_broadcast_assistant_add_src_param *)user_data;
	struct bt_bap_bass_subgroup *subgroup_param;
	uint8_t *meta_data;
	int meta_len;
	int err;

	if (subgroup == NULL || param == NULL || param->subgroups == NULL) {
		return false;
	}

	if (param->num_subgroups >= CONFIG_BT_BAP_BASS_MAX_SUBGROUPS) {
		LOG_WRN("Too many subgroups in BASE, truncating");
		return false;
	}

	meta_len = bt_bap_base_get_subgroup_codec_meta(subgroup, &meta_data);
	if (meta_len < 0) {
		return false;
	}

	subgroup_param = &param->subgroups[param->num_subgroups];

	if ((size_t)meta_len > ARRAY_SIZE(subgroup_param->metadata)) {
		LOG_WRN("Subgroup metadata (%d octets) does not fit in %zu octets", meta_len,
			ARRAY_SIZE(subgroup_param->metadata));
		return false;
	}

	err = bt_bap_base_subgroup_foreach_bis(subgroup, add_src_bis_cb, subgroup_param);
	if (err < 0) {
		return false;
	}

	param->num_subgroups++;

	return true;
}

static uint16_t interval_to_sync_timeout(uint16_t pa_interval)
{
	uint16_t pa_timeout;

	if (pa_interval == BT_BAP_PA_INTERVAL_UNKNOWN) {
		pa_timeout = BT_GAP_PER_ADV_MAX_TIMEOUT;
	} else {
		uint32_t interval_us;
		uint32_t timeout;

		interval_us = BT_GAP_PER_ADV_INTERVAL_TO_US(pa_interval);
		timeout = BT_GAP_US_TO_PER_ADV_SYNC_TIMEOUT(interval_us) *
			  PA_SYNC_INTERVAL_TO_TIMEOUT_RATIO;

		pa_timeout = CLAMP(timeout, BT_GAP_PER_ADV_MIN_TIMEOUT, BT_GAP_PER_ADV_MAX_TIMEOUT);
	}

	return pa_timeout;
}

/* Attempts to add the selected source to one sink, if everything is ready for it: that sink
 * must be the one a caller already marked busy for this (ctx->busy), connected, BASS-discovered,
 * not already configured, and the source must be selected and PA-synced. Silently returns
 * (leaving ctx->busy set) if not everything is ready yet; try_add_source_all() retries once
 * the source's BASE arrives. Must be called without state_mutex held.
 */
static void attempt_add_source(struct sink_ctx *ctx)
{
	struct bt_bap_broadcast_assistant_add_src_param param = {0};
	struct bt_conn *conn_snapshot = NULL;
	struct ble_broadcast_source source_snapshot;
	bool ready;
	int err;

	if (ctx == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ready = ctx->in_use && ctx->busy && ctx->conn != NULL && ctx->bass_discovered &&
		!ctx->configured && source_base_ready && source_selected;
	if (ready) {
		conn_snapshot = ctx->conn;
		source_snapshot = selected_source;
	}
	k_mutex_unlock(&state_mutex);

	if (!ready) {
		return;
	}

	memset(add_src_subgroups, 0, sizeof(add_src_subgroups));
	bt_addr_le_copy(&param.addr, &source_snapshot.addr);
	param.adv_sid = source_snapshot.sid;
	param.pa_interval = source_snapshot.pa_interval;
	param.broadcast_id = source_snapshot.broadcast_id;
	param.pa_sync = true;
	param.subgroups = add_src_subgroups;
	param.num_subgroups = 0U;

	err = k_mutex_lock(&base_mutex, K_MSEC(100));
	if (err != 0) {
		LOG_WRN("Could not lock BASE mutex, retrying on next event");
		return;
	}

	if (received_base_size > 0U) {
		err = bt_bap_base_foreach_subgroup((const struct bt_bap_base *)received_base,
						   add_src_subgroup_cb, &param);
	} else {
		err = -ENODATA;
	}

	(void)k_mutex_unlock(&base_mutex);

	if (err != 0) {
		LOG_ERR("Failed to build subgroup parameters (err %d)", err);
		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->busy = false;
		k_mutex_unlock(&state_mutex);
		notify_op_failed_ctx(ctx, BLE_SCAN_CTRL_OP_ADD_SOURCE, err);
		return;
	}

	err = bt_bap_broadcast_assistant_add_src(conn_snapshot, &param);
	if (err != 0) {
		LOG_ERR("Failed to add source (err %d)", err);
		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->busy = false;
		k_mutex_unlock(&state_mutex);
		notify_op_failed_ctx(ctx, BLE_SCAN_CTRL_OP_ADD_SOURCE, err);
	}
	/* else: wait for bap_add_src_cb(). */
}

/** Retries attempt_add_source() for every sink currently waiting on the source's BASE. */
static void try_add_source_all(void)
{
	for (uint8_t i = 0U; i < BLE_SCAN_CTRL_MAX_SINK_CONN; i++) {
		attempt_add_source(&sink_ctxs[i]);
	}
}

static void scan_recv_cb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
	struct adv_parse_result result = {0};
	enum scan_mode mode_snapshot;
	struct ble_broadcast_source new_source;
	struct ble_broadcast_sink new_sink;
	bool report_source = false;
	bool report_sink = false;
	uint8_t report_index = 0U;

	if (info == NULL || ad == NULL || info->addr == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	mode_snapshot = current_scan_mode;
	k_mutex_unlock(&state_mutex);

	if (mode_snapshot == SCAN_MODE_SOURCE) {
		/* Broadcast sources are non-connectable periodic advertisers. */
		if ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0 || info->interval == 0) {
			return;
		}

		result.broadcast_id = BT_BAP_INVALID_BROADCAST_ID;
		bt_data_parse(ad, device_found, (void *)&result);

		if (result.broadcast_id == BT_BAP_INVALID_BROADCAST_ID) {
			return;
		}

		k_mutex_lock(&state_mutex, K_FOREVER);

		if (!addr_in_source_list(info->addr) &&
		    found_source_count < BLE_SCAN_CTRL_MAX_RESULTS) {
			struct ble_broadcast_source *entry = &found_sources[found_source_count];

			if (result.name[0] != '\0') {
				safe_copy_name(entry->name, sizeof(entry->name),
					       (const uint8_t *)result.name,
					       (uint8_t)strlen(result.name));
			} else {
				safe_copy_name(entry->name, sizeof(entry->name),
					       (const uint8_t *)result.broadcast_name,
					       (uint8_t)strlen(result.broadcast_name));
			}
			entry->broadcast_id = result.broadcast_id;
			bt_addr_le_copy(&entry->addr, info->addr);
			entry->sid = info->sid;
			entry->pa_interval = info->interval;

			new_source = *entry;
			report_index = found_source_count;
			report_source = true;

			found_source_count++;
		}

		k_mutex_unlock(&state_mutex);

		if (report_source) {
			LOG_INF("Broadcast source found: %s (id 0x%06x)", new_source.name,
				new_source.broadcast_id);

			if (app_cb != NULL && app_cb->source_found != NULL) {
				app_cb->source_found(report_index, &new_source);
			}
		}
	} else if (mode_snapshot == SCAN_MODE_SINK) {
		/* Broadcast sinks are connectable devices exposing BASS + PACS. */
		if ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) == 0) {
			return;
		}

		bt_data_parse(ad, device_found, (void *)&result);

		if (!result.has_bass || !result.has_pacs) {
			return;
		}

		k_mutex_lock(&state_mutex, K_FOREVER);

		if (!addr_in_sink_list(info->addr) &&
		    found_sink_count < BLE_SCAN_CTRL_MAX_RESULTS) {
			struct ble_broadcast_sink *entry = &found_sinks[found_sink_count];

			safe_copy_name(entry->name, sizeof(entry->name),
				       (const uint8_t *)result.name, (uint8_t)strlen(result.name));
			bt_addr_le_copy(&entry->addr, info->addr);

			new_sink = *entry;
			report_index = found_sink_count;
			report_sink = true;

			found_sink_count++;
		}

		k_mutex_unlock(&state_mutex);

		if (report_sink) {
			LOG_INF("Broadcast sink found: %s", new_sink.name);

			if (app_cb != NULL && app_cb->sink_found != NULL) {
				app_cb->sink_found(report_index, &new_sink);
			}
		}
	}
	/* SCAN_MODE_IDLE: not currently scanning for anything meaningful; ignore. */
}

static void scan_timeout_cb(void)
{
	enum scan_mode mode_snapshot;

	k_mutex_lock(&state_mutex, K_FOREVER);
	mode_snapshot = current_scan_mode;
	k_mutex_unlock(&state_mutex);

	LOG_WRN("Scan timed out");

	if (mode_snapshot == SCAN_MODE_SOURCE) {
		notify_op_failed(BLE_SCAN_CTRL_OP_SOURCE_SCAN, -ETIMEDOUT);
	} else if (mode_snapshot == SCAN_MODE_SINK) {
		notify_op_failed(BLE_SCAN_CTRL_OP_SINK_SCAN, -ETIMEDOUT);
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv_cb,
	.timeout = scan_timeout_cb,
};

/* Aborts a freshly-connecting sink that failed before ever being configured: notifies the
 * failure, then disconnects so disconnected_cb() can drop the reference and free ctx once
 * that completes (ctx->busy stays set until then, so a retry can't start prematurely). Must
 * be called without state_mutex held.
 */
static void abandon_ctx(struct sink_ctx *ctx, enum ble_scan_ctrl_op op, int err)
{
	struct bt_conn *conn_snapshot;

	if (ctx == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx->intentional_disconnect = true;
	conn_snapshot = ctx->conn;
	k_mutex_unlock(&state_mutex);

	notify_op_failed_ctx(ctx, op, err);

	if (conn_snapshot != NULL) {
		(void)bt_conn_disconnect(conn_snapshot, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

/* Resumes sink scanning if that's still what we should be doing (i.e. not exited/reset,
 * and not scanning for sources instead) - e.g. once a connection attempt concludes and the
 * controller's initiator role is no longer tying up the radio. Never touches found_sinks[];
 * this is a plain resume, not ble_scan_ctrl_start_sink_scan()'s fresh scan.
 */
static void resume_sink_scan_if_needed(void)
{
	bool should_scan;
	int err;

	k_mutex_lock(&state_mutex, K_FOREVER);
	should_scan = (current_scan_mode == SCAN_MODE_SINK);
	k_mutex_unlock(&state_mutex);

	if (!should_scan) {
		return;
	}

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
	if (err != 0 && err != -EALREADY) {
		LOG_WRN("Failed to resume sink scan (err %d)", err);
	}
}

static void connected_cb(struct bt_conn *conn, uint8_t hci_err)
{
	struct sink_ctx *ctx;
	int sec_err;

	if (conn == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	/* This connection attempt (successful or not) is what was holding the controller's
	 * initiator role, which is why ble_scan_ctrl_select_sink() paused sink scanning for
	 * it; resume now that it is no longer needed for this attempt.
	 */
	resume_sink_scan_if_needed();

	if (hci_err != 0) {
		bool notify;

		LOG_ERR("Failed to connect to sink (err %u)", hci_err);

		k_mutex_lock(&state_mutex, K_FOREVER);
		notify = !ctx->intentional_disconnect;
		k_mutex_unlock(&state_mutex);

		/* If ble_scan_ctrl_reset() canceled this connection attempt itself (it marks
		 * intentional_disconnect before disconnecting), this is expected - don't
		 * report it as a failure.
		 */
		if (notify) {
			notify_op_failed_ctx(ctx, BLE_SCAN_CTRL_OP_SINK_CONNECT, -(int)hci_err);
		}

		k_mutex_lock(&state_mutex, K_FOREVER);
		bt_conn_drop(&ctx->conn);
		free_ctx(ctx);
		k_mutex_unlock(&state_mutex);
		return;
	}

	LOG_INF("Sink connected, requesting security");

	sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (sec_err != 0) {
		LOG_ERR("Failed to set security level (err %d)", sec_err);
		abandon_ctx(ctx, BLE_SCAN_CTRL_OP_SINK_SECURITY, sec_err);
	}
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	struct sink_ctx *ctx;
	bool notify = false;

	if (conn == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	if (ctx != NULL) {
		notify = !ctx->intentional_disconnect;
	}
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	LOG_INF("Sink disconnected (reason 0x%02x)", reason);

	if (notify && app_cb != NULL) {
		notify_indexed(ctx, app_cb->sink_disconnected);
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	bt_conn_drop(&ctx->conn);
	free_ctx(ctx);
	k_mutex_unlock(&state_mutex);
}

static void security_changed_cb(struct bt_conn *conn, bt_security_t level,
				enum bt_security_err sec_err)
{
	struct sink_ctx *ctx;
	int err;

	if (conn == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	if (sec_err != BT_SECURITY_ERR_SUCCESS) {
		LOG_ERR("Security setup failed (err %d)", (int)sec_err);
		abandon_ctx(ctx, BLE_SCAN_CTRL_OP_SINK_SECURITY, -EACCES);
		return;
	}

	LOG_INF("Security level %u established, discovering BASS", (unsigned int)level);

	err = bt_bap_broadcast_assistant_discover(conn);
	if (err != 0) {
		LOG_ERR("Failed to discover BASS (err %d)", err);
		abandon_ctx(ctx, BLE_SCAN_CTRL_OP_BASS_DISCOVER, err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected_cb,
	.disconnected = disconnected_cb,
	.security_changed = security_changed_cb,
};

/* Called once removing the source from ctx's sink has concluded: removed, found already
 * absent, or failed outright. Leaves the connection itself alone either way - disconnecting
 * here was tried and reverted (see the README's "Multiple sinks" section): on hardware, one
 * sink kept streaming after the removal regardless, and disconnecting it then left it unable
 * to accept a new connection at all until it was power-cycled - strictly worse, since nothing
 * this client does can force that sink to actually stop. Must be called without state_mutex
 * held.
 */
static void finish_remove(struct sink_ctx *ctx, bool removed, int err)
{
	if (ctx == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	if (removed) {
		ctx->configured = false;
		ctx->has_src_id = false;
	}
	ctx->busy = false;
	k_mutex_unlock(&state_mutex);

	if (removed) {
		if (app_cb != NULL) {
			notify_indexed(ctx, app_cb->source_removed);
		}
	} else {
		notify_op_failed_ctx(ctx, BLE_SCAN_CTRL_OP_REMOVE_SOURCE, err);
	}
}

/* Kicks off reading back ctx's sink's receive states to learn (or refresh) the BASS Source ID
 * for the selected source. If then_remove, removes it as soon as (or if) it is found; otherwise
 * this just warms ctx->src_id for a later removal and stays silent either way. Assumes the
 * caller already holds ctx->busy for this step (attempt_add_source()'s post-add cache warm, or
 * start_remove_source()'s fallback). Must be called without state_mutex held.
 */
static void start_learn_src_id(struct sink_ctx *ctx, bool then_remove)
{
	int err;
	struct bt_conn *conn_snapshot = NULL;
	bool have_states = false;

	k_mutex_lock(&state_mutex, K_FOREVER);
	if (ctx->conn != NULL && ctx->recv_state_count > 0U) {
		ctx->learning_src_id = true;
		ctx->learn_then_remove = then_remove;
		ctx->learn_recv_state_idx = 0U;
		conn_snapshot = ctx->conn;
		have_states = true;
	}
	k_mutex_unlock(&state_mutex);

	if (!have_states) {
		if (then_remove) {
			/* Nothing on this sink to remove; treat as already done. */
			finish_remove(ctx, true, 0);
		} else {
			k_mutex_lock(&state_mutex, K_FOREVER);
			ctx->busy = false;
			k_mutex_unlock(&state_mutex);
		}
		return;
	}

	err = bt_bap_broadcast_assistant_read_recv_state(conn_snapshot, 0U);
	if (err != 0) {
		LOG_ERR("Failed to read receive state (err %d)", err);

		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->learning_src_id = false;
		if (!then_remove) {
			ctx->busy = false;
		}
		k_mutex_unlock(&state_mutex);

		if (then_remove) {
			finish_remove(ctx, false, err);
		}
	}
}

static void bap_discover_cb(struct bt_conn *conn, int err, uint8_t recv_state_count)
{
	struct sink_ctx *ctx;

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	if (err != 0) {
		LOG_ERR("BASS discover failed (err %d)", err);
		abandon_ctx(ctx, BLE_SCAN_CTRL_OP_BASS_DISCOVER, err);
		return;
	}

	LOG_INF("BASS discovered (%u receive state(s))", recv_state_count);

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx->bass_discovered = true;
	ctx->recv_state_count = recv_state_count;
	k_mutex_unlock(&state_mutex);

	if (app_cb != NULL) {
		notify_indexed(ctx, app_cb->sink_connected);
	}

	attempt_add_source(ctx);
}

static void bap_recv_state_cb(struct bt_conn *conn, int err,
			      const struct bt_bap_scan_delegator_recv_state *state)
{
	struct sink_ctx *ctx;
	bool learning;
	bool then_remove = false;
	uint32_t target_broadcast_id = 0U;
	bool is_match;
	uint8_t next_idx = 0U;
	bool more_to_check = false;
	bool done = false;

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	learning = (ctx != NULL && ctx->learning_src_id);
	if (learning) {
		target_broadcast_id = selected_source.broadcast_id;
	}
	k_mutex_unlock(&state_mutex);

	if (!learning) {
		/* Not something we asked for (or a stray notification); ignore. */
		return;
	}

	if (err != 0) {
		LOG_ERR("Failed to read receive state (err %d)", err);

		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->learning_src_id = false;
		then_remove = ctx->learn_then_remove;
		if (!then_remove) {
			ctx->busy = false;
		}
		k_mutex_unlock(&state_mutex);

		if (then_remove) {
			finish_remove(ctx, false, err);
		}
		return;
	}

	is_match = (state != NULL && state->broadcast_id == target_broadcast_id);

	if (is_match) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->has_src_id = true;
		ctx->src_id = state->src_id;
		ctx->learning_src_id = false;
		then_remove = ctx->learn_then_remove;
		if (!then_remove) {
			ctx->busy = false;
		}
		k_mutex_unlock(&state_mutex);

		if (!then_remove) {
			/* Just warming the cache after an add; that operation is now done. */
			return;
		}

		err = bt_bap_broadcast_assistant_rem_src(conn, state->src_id);
		if (err != 0) {
			LOG_ERR("Failed to remove source (err %d)", err);
			finish_remove(ctx, false, err);
		}
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx->learn_recv_state_idx++;
	if (ctx->learn_recv_state_idx < ctx->recv_state_count) {
		next_idx = ctx->learn_recv_state_idx;
		more_to_check = true;
	} else {
		ctx->learning_src_id = false;
		then_remove = ctx->learn_then_remove;
		if (!then_remove) {
			ctx->busy = false;
		}
		done = true;
	}
	k_mutex_unlock(&state_mutex);

	if (more_to_check) {
		err = bt_bap_broadcast_assistant_read_recv_state(conn, next_idx);
		if (err != 0) {
			LOG_ERR("Failed to read receive state (err %d)", err);

			k_mutex_lock(&state_mutex, K_FOREVER);
			ctx->learning_src_id = false;
			then_remove = ctx->learn_then_remove;
			if (!then_remove) {
				ctx->busy = false;
			}
			k_mutex_unlock(&state_mutex);

			if (then_remove) {
				finish_remove(ctx, false, err);
			}
		}
		return;
	}

	if (done && then_remove) {
		/* Checked every receive state on this sink; none were for our source. */
		finish_remove(ctx, true, 0);
	}
}

static void bap_add_src_cb(struct bt_conn *conn, int err)
{
	struct sink_ctx *ctx;

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	if (err != 0) {
		LOG_ERR("BASS add source failed (err %d)", err);
		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->busy = false;
		k_mutex_unlock(&state_mutex);
		notify_op_failed_ctx(ctx, BLE_SCAN_CTRL_OP_ADD_SOURCE, err);
		return;
	}

	LOG_INF("Source added to sink");

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx->configured = true;
	k_mutex_unlock(&state_mutex);

	if (app_cb != NULL) {
		notify_indexed(ctx, app_cb->source_added);
	}

	/* Learn the BASS Source ID now, while still busy, so a future removal is instant. */
	start_learn_src_id(ctx, false);
}

static void bap_rem_src_cb(struct bt_conn *conn, int err)
{
	struct sink_ctx *ctx;

	k_mutex_lock(&state_mutex, K_FOREVER);
	ctx = find_ctx_by_conn(conn);
	k_mutex_unlock(&state_mutex);

	if (ctx == NULL) {
		return;
	}

	if (err != 0) {
		LOG_ERR("Failed to remove source (err %d)", err);
		finish_remove(ctx, false, err);
		return;
	}

	LOG_INF("Source removed from sink");

	finish_remove(ctx, true, 0);
}

static struct bt_bap_broadcast_assistant_cb bap_callbacks = {
	.discover = bap_discover_cb,
	.recv_state = bap_recv_state_cb,
	.add_src = bap_add_src_cb,
	.rem_src = bap_rem_src_cb,
};

static void pa_recv_cb(struct bt_le_per_adv_sync *sync,
		       const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf)
{
	bool is_relevant;
	bool became_ready = false;

	ARG_UNUSED(info);

	k_mutex_lock(&state_mutex, K_FOREVER);
	is_relevant = (sync != NULL && sync == pa_sync && !source_base_ready);
	k_mutex_unlock(&state_mutex);

	if (!is_relevant || buf == NULL) {
		return;
	}

	bt_data_parse(buf, base_store, NULL);

	if (received_base_size > 0U) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		if (!source_base_ready) {
			source_base_ready = true;
			became_ready = true;
		}
		k_mutex_unlock(&state_mutex);
	}

	if (became_ready) {
		LOG_INF("Source BASE received");

		if (app_cb != NULL && app_cb->source_synced != NULL) {
			app_cb->source_synced();
		}

		try_add_source_all();
	}
}

static void pa_synced_cb(struct bt_le_per_adv_sync *sync,
			 struct bt_le_per_adv_sync_synced_info *info)
{
	bool is_ours;

	ARG_UNUSED(info);

	k_mutex_lock(&state_mutex, K_FOREVER);
	is_ours = (sync != NULL && sync == pa_sync);
	k_mutex_unlock(&state_mutex);

	if (is_ours) {
		LOG_INF("Periodic advertising sync established");
	}
}

static void pa_term_cb(struct bt_le_per_adv_sync *sync,
		       const struct bt_le_per_adv_sync_term_info *info)
{
	bool is_ours;

	ARG_UNUSED(info);

	/* ble_scan_ctrl_reset() nulls pa_sync itself before deleting the sync, so by the time
	 * that intentional deletion's own termination event arrives here, is_ours is already
	 * false and this is a no-op - the same pattern used for sink connections/disconnected_cb.
	 * So reaching the notify below means the source went away on its own (switched off,
	 * went out of range, or the controller gave up), not that we tore it down ourselves.
	 */
	k_mutex_lock(&state_mutex, K_FOREVER);
	is_ours = (sync != NULL && sync == pa_sync);
	if (is_ours) {
		pa_sync = NULL;
		source_base_ready = false;
	}
	k_mutex_unlock(&state_mutex);

	if (is_ours) {
		LOG_WRN("Periodic advertising sync to the source terminated unexpectedly");
		notify_op_failed(BLE_SCAN_CTRL_OP_SOURCE_PA_SYNC, -ENOTCONN);
	}
}

static struct bt_le_per_adv_sync_cb pa_sync_callbacks = {
	.synced = pa_synced_cb,
	.term = pa_term_cb,
	.recv = pa_recv_cb,
};

int ble_scan_ctrl_init(const struct ble_scan_ctrl_cb *cb)
{
	int err;

	if (cb == NULL) {
		return -EINVAL;
	}

	app_cb = cb;

	k_mutex_init(&state_mutex);
	k_mutex_init(&base_mutex);

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	err = bt_conn_cb_register(&conn_callbacks);
	if (err != 0) {
		LOG_ERR("Failed to register connection callbacks (err %d)", err);
		return err;
	}

	bt_le_scan_cb_register(&scan_callbacks);
	bt_le_per_adv_sync_cb_register(&pa_sync_callbacks);

	err = bt_bap_broadcast_assistant_register_cb(&bap_callbacks);
	if (err != 0) {
		LOG_ERR("Failed to register Broadcast Assistant callbacks (err %d)", err);
		return err;
	}

	LOG_INF("Broadcast Assistant control module initialized");

	return 0;
}

int ble_scan_ctrl_start_source_scan(void)
{
	int err;

	k_mutex_lock(&state_mutex, K_FOREVER);
	found_source_count = 0U;
	memset(found_sources, 0, sizeof(found_sources));
	current_scan_mode = SCAN_MODE_SOURCE;
	k_mutex_unlock(&state_mutex);

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
	if (err != 0) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		current_scan_mode = SCAN_MODE_IDLE;
		k_mutex_unlock(&state_mutex);

		LOG_ERR("Failed to start source scan (err %d)", err);
		return err;
	}

	LOG_INF("Scanning for broadcast sources");

	return 0;
}

int ble_scan_ctrl_start_sink_scan(void)
{
	int err;

	k_mutex_lock(&state_mutex, K_FOREVER);
	found_sink_count = 0U;
	memset(found_sinks, 0, sizeof(found_sinks));
	current_scan_mode = SCAN_MODE_SINK;
	k_mutex_unlock(&state_mutex);

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
	if (err != 0) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		current_scan_mode = SCAN_MODE_IDLE;
		k_mutex_unlock(&state_mutex);

		LOG_ERR("Failed to start sink scan (err %d)", err);
		return err;
	}

	LOG_INF("Scanning for broadcast sinks");

	return 0;
}

int ble_scan_ctrl_select_source(uint8_t index)
{
	int err;
	struct ble_broadcast_source target;
	struct bt_le_per_adv_sync_param create_params = {0};

	k_mutex_lock(&state_mutex, K_FOREVER);

	if (index >= found_source_count || index >= BLE_SCAN_CTRL_MAX_RESULTS) {
		k_mutex_unlock(&state_mutex);
		return -EINVAL;
	}

	target = found_sources[index];
	selected_source = target;
	source_selected = true;
	source_base_ready = false;
	current_scan_mode = SCAN_MODE_IDLE;

	k_mutex_unlock(&state_mutex);

	(void)k_mutex_lock(&base_mutex, K_FOREVER);
	received_base_size = 0U;
	(void)k_mutex_unlock(&base_mutex);

	(void)bt_le_scan_stop();

	LOG_INF("Selected source: %s (id 0x%06x)", target.name, target.broadcast_id);

	bt_addr_le_copy(&create_params.addr, &target.addr);
	/* Not BT_LE_PER_ADV_SYNC_OPT_FILTER_DUPLICATE: bt_le_per_adv_sync_create() rejects it
	 * with -ENOTSUP unless the controller reports the periodic-advertising ADI feature bit
	 * (BT_FEAT_LE_PER_ADV_ADI_SUPP), which this SoC's current SoftDevice Controller firmware
	 * does not. It is only a convenience filter; pa_recv_cb() already ignores PA reports
	 * once source_base_ready is set, so duplicates are harmless without it.
	 */
	create_params.options = 0;
	create_params.sid = target.sid;
	create_params.skip = PA_SYNC_SKIP;
	create_params.timeout = interval_to_sync_timeout(target.pa_interval);

	k_mutex_lock(&state_mutex, K_FOREVER);
	err = bt_le_per_adv_sync_create(&create_params, &pa_sync);
	k_mutex_unlock(&state_mutex);

	if (err != 0) {
		/* Not notify_op_failed(): this is a synchronous failure and the caller already
		 * gets it via the return value. Calling both meant the app handled the same
		 * failure twice - once via op_failed (which resets and restarts scanning) and
		 * once via this return value (which showed its own status) - racing each other.
		 */
		LOG_ERR("Failed to create periodic advertising sync (err %d)", err);
		return err;
	}

	return 0;
}

/* Starts removing the source from ctx's sink: directly, if its BASS Source ID is already
 * cached, otherwise via start_learn_src_id()'s read-then-remove fallback. Assumes the caller
 * already validated ctx (configured, not busy) and set ctx->busy. Must be called without
 * state_mutex held.
 */
static int start_remove_source(struct sink_ctx *ctx)
{
	bool has_id;
	uint8_t cached_src_id = 0U;
	struct bt_conn *conn_snapshot;
	int err;

	k_mutex_lock(&state_mutex, K_FOREVER);
	has_id = ctx->has_src_id;
	cached_src_id = ctx->src_id;
	conn_snapshot = ctx->conn;
	k_mutex_unlock(&state_mutex);

	if (conn_snapshot == NULL) {
		k_mutex_lock(&state_mutex, K_FOREVER);
		ctx->busy = false;
		k_mutex_unlock(&state_mutex);
		return -ENOTCONN;
	}

	if (!has_id) {
		start_learn_src_id(ctx, true);
		return 0;
	}

	err = bt_bap_broadcast_assistant_rem_src(conn_snapshot, cached_src_id);
	if (err != 0) {
		LOG_ERR("Failed to remove source (err %d)", err);
		finish_remove(ctx, false, err);
		return err;
	}

	return 0;
}

int ble_scan_ctrl_select_sink(uint8_t index)
{
	bt_addr_le_t addr;
	char name_snapshot[BLE_SCAN_CTRL_NAME_LEN];
	struct sink_ctx *ctx;
	bool is_fresh = false;
	bool do_add = false;
	bool do_remove = false;
	int err;

	k_mutex_lock(&state_mutex, K_FOREVER);

	if (index >= found_sink_count || index >= BLE_SCAN_CTRL_MAX_RESULTS) {
		k_mutex_unlock(&state_mutex);
		return -EINVAL;
	}

	addr = found_sinks[index].addr;
	memcpy(name_snapshot, found_sinks[index].name, sizeof(name_snapshot));
	ctx = find_ctx_by_addr(&addr);

	if (ctx != NULL) {
		if (ctx->busy) {
			k_mutex_unlock(&state_mutex);
			return -EBUSY;
		}

		ctx->busy = true;
		if (ctx->configured) {
			do_remove = true;
		} else {
			do_add = true;
		}
	} else {
		ctx = alloc_ctx(&addr);
		if (ctx == NULL) {
			k_mutex_unlock(&state_mutex);
			return -ENOMEM;
		}

		ctx->busy = true;
		is_fresh = true;
	}

	k_mutex_unlock(&state_mutex);

	if (is_fresh) {
		/* This controller can't scan and initiate a connection at the same time
		 * (observed on hardware: bt_conn_le_create() = -EAGAIN otherwise). Sink
		 * scanning resumes from connected_cb() once this attempt concludes -
		 * found_sinks[] is untouched either way, so nothing already listed is lost.
		 */
		(void)bt_le_scan_stop();

		err = bt_conn_le_create(&addr, BT_CONN_LE_CREATE_CONN, BT_BAP_CONN_PARAM_RELAXED,
					&ctx->conn);
		if (err != 0) {
			LOG_ERR("Failed to create connection (err %d)", err);
			k_mutex_lock(&state_mutex, K_FOREVER);
			free_ctx(ctx);
			k_mutex_unlock(&state_mutex);
			resume_sink_scan_if_needed();
			return err;
		}

		LOG_INF("Connecting to sink: %s", name_snapshot);
		return 0;
	}

	if (do_add) {
		LOG_INF("Adding source to sink: %s", name_snapshot);
		attempt_add_source(ctx);
		return 0;
	}

	LOG_INF("Removing source from sink: %s", name_snapshot);

	return start_remove_source(ctx);
}

int ble_scan_ctrl_reset(void)
{
	struct bt_conn *conns_to_disconnect[BLE_SCAN_CTRL_MAX_SINK_CONN] = {0};
	uint8_t disconnect_count = 0U;
	struct bt_le_per_adv_sync *sync_to_delete;

	(void)bt_le_scan_stop();

	k_mutex_lock(&state_mutex, K_FOREVER);

	current_scan_mode = SCAN_MODE_IDLE;
	sync_to_delete = pa_sync;

	for (uint8_t i = 0U; i < BLE_SCAN_CTRL_MAX_SINK_CONN; i++) {
		if (sink_ctxs[i].in_use && sink_ctxs[i].conn != NULL) {
			sink_ctxs[i].intentional_disconnect = true;
			conns_to_disconnect[disconnect_count] = sink_ctxs[i].conn;
			disconnect_count++;
		}
	}

	source_selected = false;
	source_base_ready = false;
	found_source_count = 0U;
	found_sink_count = 0U;
	memset(&selected_source, 0, sizeof(selected_source));
	memset(found_sources, 0, sizeof(found_sources));
	memset(found_sinks, 0, sizeof(found_sinks));

	k_mutex_unlock(&state_mutex);

	(void)k_mutex_lock(&base_mutex, K_FOREVER);
	received_base_size = 0U;
	(void)k_mutex_unlock(&base_mutex);

	for (uint8_t i = 0U; i < disconnect_count; i++) {
		(void)bt_conn_disconnect(conns_to_disconnect[i], BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}

	if (sync_to_delete != NULL) {
		(void)bt_le_per_adv_sync_delete(sync_to_delete);

		k_mutex_lock(&state_mutex, K_FOREVER);
		if (pa_sync == sync_to_delete) {
			pa_sync = NULL;
		}
		k_mutex_unlock(&state_mutex);
	}

	return 0;
}
