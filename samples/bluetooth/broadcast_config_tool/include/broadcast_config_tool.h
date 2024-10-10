/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _BROADCAST_CONFIG_TOOL_H_
#define _BROADCAST_CONFIG_TOOL_H_

#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap_lc3_preset.h>

#define PRESET_NAME_MAX 8
#define LANGUAGE_LEN	3

/* The location and context type will be set by further down */
/* Low latency settings */
static struct bt_bap_lc3_preset lc3_preset_8_1_1 =
	BT_BAP_LC3_BROADCAST_PRESET_8_1_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_8_2_1 =
	BT_BAP_LC3_BROADCAST_PRESET_8_2_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_16_1_1 =
	BT_BAP_LC3_BROADCAST_PRESET_16_1_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_16_2_1 =
	BT_BAP_LC3_BROADCAST_PRESET_16_2_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_24_1_1 =
	BT_BAP_LC3_BROADCAST_PRESET_24_1_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_24_2_1 =
	BT_BAP_LC3_BROADCAST_PRESET_24_2_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_32_1_1 =
	BT_BAP_LC3_BROADCAST_PRESET_32_1_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_32_2_1 =
	BT_BAP_LC3_BROADCAST_PRESET_32_2_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_1_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_1_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_2_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_2_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_3_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_3_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_4_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_4_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_5_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_5_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_6_1 =
	BT_BAP_LC3_BROADCAST_PRESET_48_6_1(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
/* High reliability settings */
static struct bt_bap_lc3_preset lc3_preset_8_1_2 =
	BT_BAP_LC3_BROADCAST_PRESET_8_1_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_8_2_2 =
	BT_BAP_LC3_BROADCAST_PRESET_8_2_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_16_1_2 =
	BT_BAP_LC3_BROADCAST_PRESET_16_1_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_16_2_2 =
	BT_BAP_LC3_BROADCAST_PRESET_16_2_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_24_1_2 =
	BT_BAP_LC3_BROADCAST_PRESET_24_1_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_24_2_2 =
	BT_BAP_LC3_BROADCAST_PRESET_24_2_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_32_1_2 =
	BT_BAP_LC3_BROADCAST_PRESET_32_1_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_32_2_2 =
	BT_BAP_LC3_BROADCAST_PRESET_32_2_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_1_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_1_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_2_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_2_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_3_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_3_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_4_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_4_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_5_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_5_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);
static struct bt_bap_lc3_preset lc3_preset_48_6_2 =
	BT_BAP_LC3_BROADCAST_PRESET_48_6_2(BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_ANY);

struct bap_preset {
	struct bt_bap_lc3_preset *preset;
	char name[PRESET_NAME_MAX];
};

static struct bap_preset bap_presets[] = {
	{.preset = &lc3_preset_8_1_1, .name = "8_1_1"},
	{.preset = &lc3_preset_8_2_1, .name = "8_2_1"},
	{.preset = &lc3_preset_16_1_1, .name = "16_1_1"},
	{.preset = &lc3_preset_16_2_1, .name = "16_2_1"},
	{.preset = &lc3_preset_24_1_1, .name = "24_1_1"},
	{.preset = &lc3_preset_24_2_1, .name = "24_2_1"},
	{.preset = &lc3_preset_32_1_1, .name = "32_1_1"},
	{.preset = &lc3_preset_32_2_1, .name = "32_2_1"},
	{.preset = &lc3_preset_48_1_1, .name = "48_1_1"},
	{.preset = &lc3_preset_48_2_1, .name = "48_2_1"},
	{.preset = &lc3_preset_48_3_1, .name = "48_3_1"},
	{.preset = &lc3_preset_48_4_1, .name = "48_4_1"},
	{.preset = &lc3_preset_48_5_1, .name = "48_5_1"},
	{.preset = &lc3_preset_48_6_1, .name = "48_6_1"},
	{.preset = &lc3_preset_8_1_2, .name = "8_1_2"},
	{.preset = &lc3_preset_8_2_2, .name = "8_2_2"},
	{.preset = &lc3_preset_16_1_2, .name = "16_1_2"},
	{.preset = &lc3_preset_16_2_2, .name = "16_2_2"},
	{.preset = &lc3_preset_24_1_2, .name = "24_1_2"},
	{.preset = &lc3_preset_24_2_2, .name = "24_2_2"},
	{.preset = &lc3_preset_32_1_2, .name = "32_1_2"},
	{.preset = &lc3_preset_32_2_2, .name = "32_2_2"},
	{.preset = &lc3_preset_48_1_2, .name = "48_1_2"},
	{.preset = &lc3_preset_48_2_2, .name = "48_2_2"},
	{.preset = &lc3_preset_48_3_2, .name = "48_3_2"},
	{.preset = &lc3_preset_48_4_2, .name = "48_4_2"},
	{.preset = &lc3_preset_48_5_2, .name = "48_5_2"},
	{.preset = &lc3_preset_48_6_2, .name = "48_6_2"},
};

enum usecase_type {
	LECTURE,
	SILENT_TV_1,
	SILENT_TV_2,
	MULTI_LANGUAGE,
	PERSONAL_SHARING,
	PERSONAL_MULTI_LANGUAGE,
};

struct usecase_info {
	enum usecase_type use_case;
	char name[40];
};

static struct usecase_info pre_defined_use_cases[] = {
	{.use_case = LECTURE, .name = "Lecture"},
	{.use_case = SILENT_TV_1, .name = "Silent TV 1"},
	{.use_case = SILENT_TV_2, .name = "Silent TV 2"},
	{.use_case = MULTI_LANGUAGE, .name = "Multi-language"},
	{.use_case = PERSONAL_SHARING, .name = "Personal sharing"},
	{.use_case = PERSONAL_MULTI_LANGUAGE, .name = "Personal multi-language"},
};

#endif /* _BROADCAST_CONFIG_TOOL_H_ */
