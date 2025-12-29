/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "bt_content_ctrl.h"

#include <zephyr/zbus/zbus.h>
#include <zephyr/bluetooth/uuid.h>

#include "bt_content_ctrl_media_internal.h"
#include "zbus_common.h"
#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_content_ctrl, CONFIG_BT_CONTENT_CTRL_LOG_LEVEL);

ZBUS_CHAN_DEFINE(cont_media_chan, struct content_control_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

static enum content_control_evt_type state;

static void media_control_cb(bool play)
{
	int ret;
	struct content_control_msg msg;

	if (play) {
		if (state == MEDIA_STOP) {
			msg.event = MEDIA_START;
		} else {
			return;
		}
	} else {
		if (state == MEDIA_START) {
			msg.event = MEDIA_STOP;
		} else {
			return;
		}
	}

	ret = zbus_chan_pub(&cont_media_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("zbus publication failed: %d", ret);
	} else {
		state = msg.event;
	}
}

int bt_content_ctrl_start(struct bt_conn *conn)
{
	int ret;
	struct content_control_msg msg;

	if (IS_ENABLED(CONFIG_BT_MCC) || IS_ENABLED(CONFIG_BT_MCS)) {
		ret = bt_content_ctrl_media_play(conn);
		if (ret) {
			LOG_WRN("Failed to change the streaming state");
			return ret;
		}

		return 0;
	}

	msg.event = MEDIA_START;

	ret = zbus_chan_pub(&cont_media_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("zbus publication failed: %d", ret);
	}

	return 0;
}

int bt_content_ctrl_stop(struct bt_conn *conn)
{
	int ret;
	struct content_control_msg msg;

	if (IS_ENABLED(CONFIG_BT_MCC) || IS_ENABLED(CONFIG_BT_MCS)) {
		ret = bt_content_ctrl_media_pause(conn);
		if (ret) {
			LOG_WRN("Failed to change the streaming state");
			return ret;
		}

		return 0;
	}

	msg.event = MEDIA_STOP;

	ret = zbus_chan_pub(&cont_media_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("zbus publication failed: %d", ret);
	}

	return 0;
}

int bt_content_ctrl_conn_disconnected(struct bt_conn *conn)
{
	int ret;

	if (IS_ENABLED(CONFIG_BT_MCC)) {
		ret = bt_content_ctrl_media_conn_disconnected(conn);
		/* Try to reset MCS state. -ESRCH is returned if MCS hasn't been discovered
		 * yet, and shouldn't cause an error print
		 */
		if (ret && ret != -ESRCH) {
			LOG_ERR("bt_content_ctrl_media_conn_disconnected failed with %d", ret);
		}
	}

	return 0;
}

int bt_content_ctrl_discover(struct bt_conn *conn)
{
	int ret;

	if (IS_ENABLED(CONFIG_BT_MCC)) {
		ret = bt_content_ctrl_media_discover(conn);
		if (ret) {
			LOG_ERR("Failed to discover the media control client");
			return ret;
		}
	}

	return 0;
}

int bt_content_ctrl_uuid_populate(struct net_buf_simple *uuid_buf)
{
	if (IS_ENABLED(CONFIG_BT_MCC)) {
		if (net_buf_simple_tailroom(uuid_buf) >= BT_UUID_SIZE_16) {
			net_buf_simple_add_le16(uuid_buf, BT_UUID_MCS_VAL);
		} else {
			return -ENOMEM;
		}
	}

	return 0;
}

int bt_content_ctrl_init(void)
{
	int ret;

	state = MEDIA_START;

	if (IS_ENABLED(CONFIG_BT_MCS)) {
		ret = bt_content_ctrl_media_server_init(media_control_cb);
		if (ret) {
			LOG_ERR("MCS server init failed");
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_BT_MCC)) {
		ret = bt_content_ctrl_media_client_init();
		if (ret) {
			LOG_ERR("MCS client init failed");
			return ret;
		}
	}

	return 0;
}
