/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "streamctrl.h"

#include <zephyr/zbus/zbus.h>

#include "nrf5340_audio_common.h"
#include "nrf5340_audio_dk.h"
#include "led.h"
#include "button_assignments.h"
#include "macros_common.h"
#include "audio_system.h"
#include "button_handler.h"
#include "bt_le_audio_tx.h"
#include "bt_mgmt.h"
#include "bt_rendering_and_capture.h"
#include "audio_datapath.h"
#include "bt_content_ctrl.h"
#include "unicast_server.h"
#include "le_audio.h"
#include "le_audio_rx.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

#define ZBUS_PP_TIMEOUT (500)

ZBUS_SUBSCRIBER_DEFINE(button_evt_sub, CONFIG_BUTTON_MSG_SUB_QUEUE_SIZE);

ZBUS_MSG_SUBSCRIBER_DEFINE(le_audio_evt_sub);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_CHAN_DECLARE(le_audio_chan);
ZBUS_CHAN_DECLARE(bt_mgmt_chan);
ZBUS_CHAN_DECLARE(volume_chan);

ZBUS_OBS_DECLARE(volume_evt_sub);

static struct k_thread button_msg_sub_thread_data;
static struct k_thread le_audio_msg_sub_thread_data;

static k_tid_t button_msg_sub_thread_id;
static k_tid_t le_audio_msg_sub_thread_id;

K_THREAD_STACK_DEFINE(button_msg_sub_thread_stack, CONFIG_BUTTON_MSG_SUB_STACK_SIZE);
K_THREAD_STACK_DEFINE(le_audio_msg_sub_thread_stack, CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE);

static enum stream_state strm_state = STATE_PAUSED;

/* Function for handling all stream state changes */
static void stream_state_set(enum stream_state stream_state_new)
{
	strm_state = stream_state_new;
}

/**
 * @brief Number of buttons.
 */
#define CONFIG_BUTTONS_COUNT (5)

/****/
#define BUTTON_REPEAT_TRACK_NEXT  (1)
#define BUTTON_REPEAT_TRACK_PREVIOUS  (2)

/**
 * @brief The private context structure for a single button.
 */
struct button_context_private;

/**
 * @brief Structure for the buttons functions.
 */
struct button_functions
{
	/**
	 * @brief Function to execute after multiple single button presses have been detected.
	 *
	 * @param button_pressed  [in/out]  The context of the button.
	 * @param repeat_count    [in]      The number of times the button was pressed.
	 *
	 * @return 0 if successful, error otherwise.
	 */
    int (*repeat)(struct button_context_private * button_pressed, int repeat_count);

	/**
	 * @brief Function to execute after a single button press has been detected.
	 *
	 * @param button_pressed  [in/out]  The context of the button.
	 *
	 * @return 0 if successful, error otherwise.
	 */
    int (*single)(struct button_context_private * button_pressed);
};

/**
 * @brief The context structure for a single button.
 */
struct button_context
{
	/* Button pin assignment. */
    int pin;

	/* Name of the button. */
    char *name;

	/* Maximum repeat presses for the button. */
    int repeat_max;

	/* Time between repeated presses for a button. */
	int repeat_timeout;

	/* Functions to call for the button. */
    struct button_functions cb;
};

static int volume_down(struct button_context_private * button_pressed)
{
	int ret;

    ret = bt_r_and_c_volume_down();
    if (ret) {
        LOG_DBG("Failed volume down");
        return ret;
    }

    return 0;
}

static int volume_up(struct button_context_private * button_pressed)
{
	int ret;

    ret = bt_r_and_c_volume_up();
    if (ret) {
        LOG_DBG("Failed volume up");
        return ret;
    }

    return 0;
}

static int button_4(struct button_context_private * button_pressed)
{
    int ret;

    if (IS_ENABLED(CONFIG_AUDIO_TEST_TONE)) {
        if (IS_ENABLED(CONFIG_WALKIE_TALKIE_DEMO)) {
            LOG_DBG("Test tone is disabled in walkie-talkie mode");
            return -ENOTSUP;
        }

        if (strm_state != STATE_STREAMING) {
            LOG_WRN("Not in streaming state");
            return -EINVAL;
        }

        ret = audio_system_encode_test_tone_step();
        if (ret) {
            LOG_WRN("Failed to play test tone, ret: %d", ret);
            return ret;
        }
    }

    return 0;
}

static int button_5(struct button_context_private * button_pressed)
{
    int ret;

    if (IS_ENABLED(CONFIG_AUDIO_MUTE)) {
        ret = bt_r_and_c_volume_mute(false);
        if (ret) {
            LOG_WRN("Failed to mute, ret: %d", ret);
            return ret;
        }
    }

    return 0;
}

static int ppnp_single(struct button_context_private * button_pressed)
{
    int ret;

	if (IS_ENABLED(CONFIG_WALKIE_TALKIE_DEMO)) {
		LOG_WRN("Play/pause not supported in walkie-talkie mode");
		return -ENOTSUP;
	}

	if (bt_content_ctlr_media_state_playing()) {
		ret = bt_content_ctrl_stop(NULL);
		if (ret) {
			LOG_WRN("Could not stop: %d", ret);
		}

	} else if (!bt_content_ctlr_media_state_playing()) {
		ret = bt_content_ctrl_start(NULL);
		if (ret) {
			LOG_WRN("Could not start: %d", ret);
		}

	} else {
		LOG_WRN("In invalid state: %d", strm_state);
		return -EINVAL;
	}

	return ret;
}

static int ppnp_repeat(struct button_context_private *button_pressed_private, int repeat_count)
{
    int ret;

    if (IS_ENABLED(CONFIG_WALKIE_TALKIE_DEMO)) {
        LOG_WRN(
            "Next/previous track not supported in walkie-talkie and "
            "bidirectional mode");
        return -ENOTSUP;
    }

    if (repeat_count == BUTTON_REPEAT_TRACK_NEXT)  {
        LOG_INF("Button repeat action skip");
        return 0;
    } else if (repeat_count == BUTTON_REPEAT_TRACK_PREVIOUS) {
        LOG_INF("Button repeat action previous");
        return 0;
    } else {
        LOG_WRN("Unhandled button repeat action");
        return -EINVAL;
    }

    return ret;
}

/**
 * @brief Array of button contexts.
 */
static struct button_context audio_buttons_ctx[CONFIG_BUTTONS_COUNT] = {
    { BUTTON_VOLUME_DOWN, "volume down", 0, 500, .cb = { NULL, volume_down } },
    { BUTTON_VOLUME_UP, "volume up", 0, 500, .cb = { NULL, volume_up } },
    { BUTTON_PLAY_PAUSE, "play/pause/next/previous", 2, 500, .cb = { ppnp_repeat, ppnp_single } },
    { BUTTON_4, "4", 0, 500, .cb = { NULL, button_4 } },
    { BUTTON_5, "5", 0, 500, .cb = { NULL, button_5 } }
};

/**
 * @brief Find the button context index from the pin number.
 */
int get_button(struct button_context *ctx, int pin, int *button_id)
{
	struct button_context *button;

	for(int i = 0; i < CONFIG_BUTTONS_COUNT; i++) {
		button = &ctx[i];

		if (button->pin == pin) {
			*button_id = i;
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * @brief Decode button press.
 */
static int button_decode(struct button_context *button_pressed, int repeat_count)
{
    int ret = 0;

    if (repeat_count) {
        if (button_pressed->cb.repeat != NULL) {
            ret = button_pressed->cb.repeat((struct button_context_private *)button_pressed, repeat_count);
			if (ret) {
            	LOG_DBG("Failed the button %s repeated press callback", button_pressed->name);
            return ret;
        	}
		}
    } else {
        if (button_pressed->cb.single != NULL) {
            ret = button_pressed->cb.single((struct button_context_private *)button_pressed);
        	if (ret) {
            	LOG_DBG("Failed the button %s single press callback", button_pressed->name);
            	return ret;
        	}
		}
    }

    return 0;
}

/**
 * @brief	Handle button activity.
 */
static void button_msg_sub_thread(void)
{
    int ret;
    const struct zbus_channel *chan;
	struct button_msg msg;
	int repeat_count;
	int button_id;
	struct button_context *button_pressed;

    while (1) {
        ret = zbus_sub_wait(&button_evt_sub, &chan, K_FOREVER);
        ERR_CHK(ret);

        ret = zbus_chan_read(chan, &msg, K_FOREVER);
        ERR_CHK(ret);

        LOG_DBG("Got btn evt from queue - id = %d, action = %d", msg.button_pin, msg.button_action);

        ret = get_button(&audio_buttons_ctx[0], msg.button_pin, &button_id);
		if (ret) {
			 LOG_DBG("Button decode failed: %d", ret);
			 return;
		}

        button_pressed = &audio_buttons_ctx[button_id];
		LOG_DBG("Found button %d, with max repeat of %d", button_id, button_pressed->repeat_max);

        repeat_count = 0;

        while (repeat_count < button_pressed->repeat_max) {
            ret = zbus_sub_wait(&button_evt_sub, &chan, K_MSEC(button_pressed->repeat_timeout));
            if (ret == -EAGAIN) {
				LOG_DBG("Repeat timed out");
                break;
            }
            ERR_CHK(ret);

            ret = zbus_chan_read(chan, &msg, K_NO_WAIT);
            ERR_CHK(ret);

            if (msg.button_action != BUTTON_PRESS) {
                LOG_DBG("Button repeat test timedout");
                break;
            }

            if (msg.button_pin == button_pressed->pin) {
                repeat_count += 1;
                LOG_DBG("Repeat %s button press count: %d", button_pressed->name, repeat_count);
            } else {
                LOG_DBG("End repeat cycle as %s button pressed",
                        audio_buttons_ctx[msg.button_pin].name);
                break;
            }
        }

		LOG_DBG("Number of repeat presses %d", repeat_count);

        ret = button_decode(button_pressed, repeat_count);
        if (ret) {
            LOG_DBG("Button %s decode failed: %d", audio_buttons_ctx[msg.button_pin].name, ret);
        }

        STACK_USAGE_PRINT("button_msg_thread", &button_msg_sub_thread_data);
    }
}

/**
 * @brief	Handle Bluetooth LE audio events.
 */
static void le_audio_msg_sub_thread(void)
{
	int ret;
	uint32_t pres_delay_us;
	uint32_t bitrate_bps;
	uint32_t sampling_rate_hz;
	const struct zbus_channel *chan;

	while (1) {
		struct le_audio_msg msg;

		ret = zbus_sub_wait_msg(&le_audio_evt_sub, &chan, &msg, K_FOREVER);
		ERR_CHK(ret);

		LOG_DBG("Received event = %d, current state = %d", msg.event, strm_state);

		switch (msg.event) {
		case LE_AUDIO_EVT_STREAMING:
			LOG_DBG("LE audio evt streaming");

			if (msg.dir == BT_AUDIO_DIR_SOURCE) {
				audio_system_encoder_start();
			}

			if (strm_state == STATE_STREAMING) {
				LOG_DBG("Got streaming event in streaming state");
				break;
			}

			audio_system_start();
			stream_state_set(STATE_STREAMING);
			ret = led_blink(LED_APP_1_BLUE);
			ERR_CHK(ret);

			break;

		case LE_AUDIO_EVT_NOT_STREAMING:
			LOG_DBG("LE audio evt not streaming");

			if (strm_state == STATE_PAUSED) {
				LOG_DBG("Got not_streaming event in paused state");
				break;
			}

			if (msg.dir == BT_AUDIO_DIR_SOURCE) {
				audio_system_encoder_stop();
			}

			stream_state_set(STATE_PAUSED);
			audio_system_stop();
			ret = led_on(LED_APP_1_BLUE);
			ERR_CHK(ret);

			break;

		case LE_AUDIO_EVT_CONFIG_RECEIVED:
			LOG_DBG("LE audio config received");

			ret = unicast_server_config_get(msg.conn, msg.dir, &bitrate_bps,
							&sampling_rate_hz, NULL);
			if (ret) {
				LOG_WRN("Failed to get config: %d", ret);
				break;
			}

			LOG_DBG("\tSampling rate: %d Hz", sampling_rate_hz);
			LOG_DBG("\tBitrate (compressed): %d bps", bitrate_bps);

			if (msg.dir == BT_AUDIO_DIR_SINK) {
				ret = audio_system_config_set(VALUE_NOT_SET, VALUE_NOT_SET,
							      sampling_rate_hz);
				ERR_CHK(ret);
			} else if (msg.dir == BT_AUDIO_DIR_SOURCE) {
				ret = audio_system_config_set(sampling_rate_hz, bitrate_bps,
							      VALUE_NOT_SET);
				ERR_CHK(ret);
			}

			break;

		case LE_AUDIO_EVT_PRES_DELAY_SET:
			LOG_DBG("Set presentation delay");

			ret = unicast_server_config_get(msg.conn, BT_AUDIO_DIR_SINK, NULL, NULL,
							&pres_delay_us);
			if (ret) {
				LOG_ERR("Failed to get config: %d", ret);
				break;
			}

			ret = audio_datapath_pres_delay_us_set(pres_delay_us);
			if (ret) {
				LOG_ERR("Failed to set presentation delay to %d", pres_delay_us);
				break;
			}

			LOG_INF("Presentation delay %d us is set by initiator", pres_delay_us);

			break;

		case LE_AUDIO_EVT_NO_VALID_CFG:
			LOG_WRN("No valid configurations found, will disconnect");

			ret = bt_mgmt_conn_disconnect(msg.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			if (ret) {
				LOG_ERR("Failed to disconnect: %d", ret);
			}

			break;

		default:
			LOG_WRN("Unexpected/unhandled le_audio event: %d", msg.event);

			break;
		}

		STACK_USAGE_PRINT("le_audio_msg_thread", &le_audio_msg_sub_thread_data);
	}
}

/**
 * @brief	Create zbus subscriber threads.
 *
 * @return	0 for success, error otherwise.
 */
static int zbus_subscribers_create(void)
{
	int ret;

	button_msg_sub_thread_id = k_thread_create(
		&button_msg_sub_thread_data, button_msg_sub_thread_stack,
		CONFIG_BUTTON_MSG_SUB_STACK_SIZE, (k_thread_entry_t)button_msg_sub_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_BUTTON_MSG_SUB_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(button_msg_sub_thread_id, "BUTTON_MSG_SUB");
	if (ret) {
		LOG_ERR("Failed to create button_msg thread");
		return ret;
	}

	le_audio_msg_sub_thread_id = k_thread_create(
		&le_audio_msg_sub_thread_data, le_audio_msg_sub_thread_stack,
		CONFIG_LE_AUDIO_MSG_SUB_STACK_SIZE, (k_thread_entry_t)le_audio_msg_sub_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_LE_AUDIO_MSG_SUB_THREAD_PRIO), 0, K_NO_WAIT);
	ret = k_thread_name_set(le_audio_msg_sub_thread_id, "LE_AUDIO_MSG_SUB");
	if (ret) {
		LOG_ERR("Failed to create le_audio_msg thread");
		return ret;
	}

	return 0;
}

/**
 * @brief	Zbus listener to receive events from bt_mgmt.
 *
 * @param[in]	chan	Zbus channel.
 *
 * @note	Will in most cases be called from BT_RX context,
 *		so there should not be too much processing done here.
 */
static void bt_mgmt_evt_handler(const struct zbus_channel *chan)
{
	int ret;
	const struct bt_mgmt_msg *msg;

	msg = zbus_chan_const_msg(chan);

	switch (msg->event) {
	case BT_MGMT_CONNECTED:
		LOG_INF("Connected");

		break;

	case BT_MGMT_DISCONNECTED:
		LOG_INF("Disconnected");

		ret = bt_content_ctrl_conn_disconnected(msg->conn);
		if (ret) {
			LOG_ERR("Failed to handle disconnection in content control: %d", ret);
		}

		break;

	case BT_MGMT_SECURITY_CHANGED:
		LOG_INF("Security changed");

		ret = bt_r_and_c_discover(msg->conn);
		if (ret) {
			LOG_WRN("Failed to discover rendering services");
		}

		ret = bt_content_ctrl_discover(msg->conn);
		if (ret == -EALREADY) {
			LOG_DBG("Discovery in progress or already done");
		} else if (ret) {
			LOG_ERR("Failed to start discovery of content control: %d", ret);
		}

		break;

	default:
		LOG_WRN("Unexpected/unhandled bt_mgmt event: %d", msg->event);

		break;
	}
}

ZBUS_LISTENER_DEFINE(bt_mgmt_evt_listen, bt_mgmt_evt_handler);

/**
 * @brief	Link zbus producers and observers.
 *
 * @return	0 for success, error otherwise.
 */
static int zbus_link_producers_observers(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_ZBUS)) {
		return -ENOTSUP;
	}

	ret = zbus_chan_add_obs(&button_chan, &button_evt_sub, ZBUS_ADD_OBS_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to add button sub");
		return ret;
	}

	ret = zbus_chan_add_obs(&le_audio_chan, &le_audio_evt_sub, ZBUS_ADD_OBS_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to add le_audio sub");
		return ret;
	}

	ret = zbus_chan_add_obs(&bt_mgmt_chan, &bt_mgmt_evt_listen, ZBUS_ADD_OBS_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to add bt_mgmt sub");
		return ret;
	}

	ret = zbus_chan_add_obs(&volume_chan, &volume_evt_sub, ZBUS_ADD_OBS_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to add volume sub");
		return ret;
	}

	return 0;
}

static int ext_adv_populate(struct bt_data *ext_adv_buf, size_t ext_adv_buf_size,
			    size_t *ext_adv_count)
{
	int ret;
	size_t ext_adv_buf_cnt = 0;

	NET_BUF_SIMPLE_DEFINE_STATIC(uuid_buf, CONFIG_EXT_ADV_UUID_BUF_MAX);

	ext_adv_buf[ext_adv_buf_cnt].type = BT_DATA_UUID16_SOME;
	ext_adv_buf[ext_adv_buf_cnt].data_len = 0;
	ext_adv_buf[ext_adv_buf_cnt].data = uuid_buf.data;
	ext_adv_buf_cnt++;

	ret = bt_r_and_c_uuid_populate(&uuid_buf);

	if (ret) {
		LOG_ERR("Failed to add adv data from renderer: %d", ret);
		return ret;
	}

	ret = bt_content_ctrl_uuid_populate(&uuid_buf);

	if (ret) {
		LOG_ERR("Failed to add adv data from content ctrl: %d", ret);
		return ret;
	}

	ret = bt_mgmt_manufacturer_uuid_populate(&uuid_buf, CONFIG_BT_DEVICE_MANUFACTURER_ID);
	if (ret) {
		LOG_ERR("Failed to add adv data with manufacturer ID: %d", ret);
		return ret;
	}

	ret = unicast_server_adv_populate(&ext_adv_buf[ext_adv_buf_cnt],
					  ext_adv_buf_size - ext_adv_buf_cnt);

	if (ret < 0) {
		LOG_ERR("Failed to add adv data from unicast server: %d", ret);
		return ret;
	}

	ext_adv_buf_cnt += ret;

	/* Add the number of UUIDs */
	ext_adv_buf[0].data_len = uuid_buf.len;

	LOG_DBG("Size of adv data: %d, num_elements: %d", sizeof(struct bt_data) * ext_adv_buf_cnt,
		ext_adv_buf_cnt);

	*ext_adv_count = ext_adv_buf_cnt;

	return 0;
}

uint8_t stream_state_get(void)
{
	return strm_state;
}

void streamctrl_send(void const *const data, size_t size, uint8_t num_ch)
{
	int ret;
	static int prev_ret;

	struct le_audio_encoded_audio enc_audio = {.data = data, .size = size, .num_ch = num_ch};

	if (strm_state == STATE_STREAMING) {
		ret = unicast_server_send(enc_audio);

		if (ret != 0 && ret != prev_ret) {
			if (ret == -ECANCELED) {
				LOG_WRN("Sending operation cancelled");
			} else {
				LOG_WRN("Problem with sending LE audio data, ret: %d", ret);
			}
		}

		prev_ret = ret;
	}
}

int main(void)
{
	int ret;
	static struct bt_data ext_adv_buf[CONFIG_EXT_ADV_BUF_MAX];

	LOG_DBG("nRF5340 APP core started");

	size_t ext_adv_buf_cnt = 0;

	ret = nrf5340_audio_dk_init();
	ERR_CHK(ret);

	ret = nrf5340_audio_common_init();
	ERR_CHK(ret);

	ret = zbus_subscribers_create();
	ERR_CHK_MSG(ret, "Failed to create zbus subscriber threads");

	ret = zbus_link_producers_observers();
	ERR_CHK_MSG(ret, "Failed to link zbus producers and observers");

	ret = le_audio_rx_init();
	ERR_CHK_MSG(ret, "Failed to initialize rx path");

	ret = unicast_server_enable(le_audio_rx_data_handler);
	ERR_CHK_MSG(ret, "Failed to enable LE Audio");

	ret = bt_r_and_c_init();
	ERR_CHK(ret);

	ret = bt_content_ctrl_init();
	ERR_CHK(ret);

	ret = ext_adv_populate(ext_adv_buf, ARRAY_SIZE(ext_adv_buf), &ext_adv_buf_cnt);
	ERR_CHK(ret);

	ret = bt_mgmt_adv_start(ext_adv_buf, ext_adv_buf_cnt, NULL, 0, true);
	ERR_CHK(ret);

	return 0;
}
