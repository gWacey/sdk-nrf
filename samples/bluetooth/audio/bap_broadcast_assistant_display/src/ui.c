/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"

#include "nordic_logo.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui, LOG_LEVEL_INF);

/*
 * All mutable state in this file (rows[], master_entries[], filter state,
 * active_screen, and of course every LVGL object) is touched both from the
 * thread that calls ui_process() and from whichever thread the application
 * drives ble_scan_ctrl callbacks on. lv_lock()/lv_unlock() (backed by a real
 * Zephyr mutex, see CONFIG_LV_Z_LVGL_MUTEX) is used as the single lock
 * protecting all of it, including our own bookkeeping, not just the LVGL
 * calls.
 *
 * Locking convention used throughout this file: every function reachable
 * directly from ui.h (ui_init, ui_show_*, ui_add_*_row, ui_process) takes
 * the lock itself for its whole body and never calls back into the
 * application while holding it. Internal helpers and LVGL event callbacks
 * assume the lock is already held by their caller (either one of the
 * functions above, or - for event callbacks - ui_process()'s call to
 * lv_timer_handler()) and do not lock again. Zephyr mutexes are recursive
 * for the owning thread, so nested locking from an event callback that
 * happens to call back into a lock-taking ui_* function is still safe.
 */

/*
 * Nordic Semiconductor brand colors, sampled directly from the computed
 * styles of nordicsemi.com (accent blue, body text) so the UI reads as
 * "Nordic" rather than an approximation from memory.
 */
#define NORDIC_COLOR_BLUE	   0x00A9CEU /* primary accent / links / brand blue */
#define NORDIC_COLOR_TEXT	   0x333F48U /* body text color */
#define NORDIC_COLOR_BG		   0xFFFFFFU /* page background */
#define NORDIC_COLOR_SURFACE	   0xF6F6F6U /* secondary surface (buttons, panels) */
/* Not from nordicsemi.com: plain neutral greys for a disabled/already-done row. */
#define NORDIC_COLOR_DISABLED_BG   0xDDDDDDU
#define NORDIC_COLOR_DISABLED_TEXT 0x9A9A9AU

/** Maximum length, including the terminating NUL, of a name kept for filter matching. */
#define UI_FILTER_NAME_LEN 40U
/** Maximum length, including the terminating NUL, of the typed filter term. */
#define UI_FILTER_TEXT_LEN 32U

/** Per-row bookkeeping; a pointer to one of these is used as the LVGL event user_data. */
struct ui_row {
	bool in_use;
	/** Index into master_entries[], NOT the row's on-screen position (which shifts as the
	 * filter changes).
	 */
	uint8_t index;
};

/** One discovered source or sink, independent of whether it currently matches the filter. */
struct ui_master_entry {
	bool in_use;
	/** Shown greyed out, e.g. a sink already configured with the source; still tappable. */
	bool greyed_out;
	char name[UI_FILTER_NAME_LEN];
	char display_text[48];
};

static ui_row_selected_cb row_selected_cb;
static ui_clock_tapped_cb clock_tapped_cb;
static ui_exit_cb exit_cb;

static lv_obj_t *splash_screen;

static lv_obj_t *clock_screen;
static lv_obj_t *clock_hour_hand;
static lv_obj_t *clock_min_hand;
static lv_obj_t *clock_sec_hand;

/*
 * Analog clock face geometry. Hand endpoints are recomputed from a
 * precomputed 60-entry direction table (index 0 = 12 o'clock, clockwise,
 * one entry per minute/second position: angle = index * 6 degrees) rather
 * than calling sinf()/cosf() at runtime, so the clock face has no
 * dependency on libm being available/linked for this target - just integer
 * multiply and divide.
 */
#define CLOCK_TABLE_SIZE     60
#define CLOCK_CENTER_X	     120
#define CLOCK_CENTER_Y	     120
#define CLOCK_TICK_RADIUS    92
#define CLOCK_HOUR_HAND_LEN  45
#define CLOCK_MIN_HAND_LEN   68
#define CLOCK_SEC_HAND_LEN   78
/* A warm red-orange, distinct from the brand-blue tick marks/title text. */
#define CLOCK_SEC_HAND_COLOR 0xE4572EU

/* dir_x[i] = round(sin(i*6deg) * 1000), dir_y[i] = round(-cos(i*6deg) * 1000) */
static const int16_t clock_dir_x[CLOCK_TABLE_SIZE] = {
	0,     105,  208,  309,	 407,  500,  588,  669,	 743,  809,  866,  914,	 951,  978,  995,
	1000,  995,  978,  951,	 914,  866,  809,  743,	 669,  588,  500,  407,	 309,  208,  105,
	0,     -105, -208, -309, -407, -500, -588, -669, -743, -809, -866, -914, -951, -978, -995,
	-1000, -995, -978, -951, -914, -866, -809, -743, -669, -588, -500, -407, -309, -208, -105,
};
static const int16_t clock_dir_y[CLOCK_TABLE_SIZE] = {
	-1000, -995, -978, -951, -914, -866, -809, -743, -669, -588, -500, -407, -309, -208, -105,
	0,     105,  208,  309,	 407,  500,  588,  669,	 743,  809,  866,  914,	 951,  978,  995,
	1000,  995,  978,  951,	 914,  866,  809,  743,	 669,  588,  500,  407,	 309,  208,  105,
	0,     -105, -208, -309, -407, -500, -588, -669, -743, -809, -866, -914, -951, -978, -995,
};

/* lv_line_set_points() stores a pointer to these, not a copy, so they must outlive the line. */
static lv_point_precise_t clock_hour_points[2];
static lv_point_precise_t clock_min_points[2];
static lv_point_precise_t clock_sec_points[2];

/*
 * RTC device backing the clock screen, from the "rtc" devicetree alias the
 * seeed_xiao_round_display shield points at its PCF8563 battery-backed RTC
 * chip. If nothing set the RTC's time (it has no network/BLE time source of
 * its own), it simply reads back whatever it powered on with; the hands
 * show whatever the RTC currently holds rather than guaranteed-correct wall
 * time.
 */
#if DT_HAS_ALIAS(rtc)
static const struct device *const clock_rtc_dev = DEVICE_DT_GET(DT_ALIAS(rtc));
#else
static const struct device *const clock_rtc_dev;
#endif
static int64_t clock_last_update_ms;

static lv_obj_t *list_screen;
static lv_obj_t *list_title_label;
static lv_obj_t *list_widget;

static lv_obj_t *filter_toggle_btn;
static lv_obj_t *filter_toggle_label;
static lv_obj_t *exit_btn;
static lv_obj_t *filter_display_btn;
static lv_obj_t *filter_display_label;
static lv_obj_t *filter_textarea;
static lv_obj_t *filter_keyboard;

static lv_obj_t *status_screen;
static lv_obj_t *status_title_label;
static lv_obj_t *status_detail_label;

static struct ui_row rows[UI_MAX_LIST_ITEMS];
static uint8_t row_count;

static struct ui_master_entry master_entries[UI_MAX_LIST_ITEMS];
static uint8_t master_count;

static char filter_text[UI_FILTER_TEXT_LEN] = "";
static bool filter_enabled;

static enum ui_screen active_screen = UI_SCREEN_CLOCK;

static char to_lower_ascii(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return (char)(c - 'A' + 'a');
	}

	return c;
}

/** Case-insensitive comparison of two characters, treated as ASCII. */
static bool chars_equal_ci(char a, char b)
{
	return to_lower_ascii(a) == to_lower_ascii(b);
}

/**
 * Case-insensitive glob match against the whole string: '*' in @p pattern matches any run
 * of characters (including none); every other character must match literally at that
 * position. Iterative (no recursion), bounded by the length of @p text - safe for the
 * short, fixed-size buffers this is always called with.
 */
static bool wildcard_match(const char *pattern, const char *text)
{
	const char *p = pattern;
	const char *t = text;
	const char *star_p = NULL;
	const char *star_t = NULL;

	if (pattern == NULL || text == NULL) {
		return false;
	}

	while (*t != '\0') {
		if (*p == '*') {
			star_p = p;
			star_t = t;
			p++;
		} else if (*p != '\0' && chars_equal_ci(*p, *t)) {
			p++;
			t++;
		} else if (star_p != NULL) {
			p = star_p + 1;
			star_t++;
			t = star_t;
		} else {
			return false;
		}
	}

	while (*p == '*') {
		p++;
	}

	return *p == '\0';
}

/**
 * Returns true if the filter is off or empty. Otherwise: if the typed filter contains a
 * '*', it's matched as a case-insensitive wildcard pattern anchored to the whole name
 * (e.g. "NRF*" = starts with, "*CASTER" = ends with, "NRF*ER" = both); if it contains no
 * '*', it's matched as a plain case-insensitive substring ("contains") search, as before.
 */
static bool name_matches_filter(const char *name)
{
	size_t name_len;
	size_t filter_len;

	if (!filter_enabled || filter_text[0] == '\0') {
		return true;
	}

	if (name == NULL) {
		return false;
	}

	if (strchr(filter_text, '*') != NULL) {
		return wildcard_match(filter_text, name);
	}

	name_len = strlen(name);
	filter_len = strlen(filter_text);

	if (filter_len > name_len) {
		return false;
	}

	for (size_t pos = 0U; pos + filter_len <= name_len; pos++) {
		size_t i;

		for (i = 0U; i < filter_len; i++) {
			if (!chars_equal_ci(name[pos + i], filter_text[i])) {
				break;
			}
		}

		if (i == filter_len) {
			return true;
		}
	}

	return false;
}

static void style_row_button(lv_obj_t *btn, bool greyed_out)
{
	if (btn == NULL) {
		return;
	}

	lv_obj_set_style_pad_all(btn, 4, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(NORDIC_COLOR_SURFACE), 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(NORDIC_COLOR_BLUE), LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn, lv_color_hex(NORDIC_COLOR_TEXT), 0);
	lv_obj_set_style_text_color(btn, lv_color_hex(NORDIC_COLOR_BG), LV_STATE_PRESSED);

	if (greyed_out) {
		/* Visual "already configured" cue only. Deliberately not LV_STATE_DISABLED,
		 * which would also make LVGL's input handling skip press/click events for this
		 * object outright (see lv_indev.c's is_enabled checks) - this row must stay
		 * tappable, since tapping it releases the source from this sink.
		 */
		lv_obj_set_style_bg_color(btn, lv_color_hex(NORDIC_COLOR_DISABLED_BG), 0);
		lv_obj_set_style_text_color(btn, lv_color_hex(NORDIC_COLOR_DISABLED_TEXT), 0);
	}
}

/* Runs on whichever thread calls ui_process(), always with the LVGL lock held. */
static void row_click_event_cb(lv_event_t *e)
{
	struct ui_row *row;
	ui_row_selected_cb cb;
	enum ui_screen screen;
	uint8_t index;

	if (e == NULL) {
		return;
	}

	row = (struct ui_row *)lv_event_get_user_data(e);

	/* Defensive bounds check: row must point inside the static rows[] array. */
	if (row == NULL || row < &rows[0] || row > &rows[UI_MAX_LIST_ITEMS - 1U]) {
		return;
	}

	if (!row->in_use || row->index >= UI_MAX_LIST_ITEMS) {
		return;
	}

	cb = row_selected_cb;
	screen = active_screen;
	index = row->index;

	if (cb != NULL) {
		cb(screen, index);
	}
}

/** Assumes the LVGL lock is already held. Appends one visible row bound to master index. */
static void create_row_button(const char *text, uint8_t master_index, bool greyed_out)
{
	lv_obj_t *btn;

	if (text == NULL || row_count >= UI_MAX_LIST_ITEMS) {
		return;
	}

	btn = lv_list_add_button(list_widget, NULL, text);
	if (btn == NULL) {
		return;
	}

	style_row_button(btn, greyed_out);

	rows[row_count].in_use = true;
	rows[row_count].index = master_index;
	lv_obj_add_event_cb(btn, row_click_event_cb, LV_EVENT_CLICKED, &rows[row_count]);

	row_count++;
}

/** Assumes the LVGL lock is already held. Rebuilds the visible list from master_entries[]. */
static void rebuild_visible_rows(void)
{
	lv_obj_clean(list_widget);
	memset(rows, 0, sizeof(rows));
	row_count = 0U;

	for (uint8_t i = 0U; i < master_count && i < UI_MAX_LIST_ITEMS; i++) {
		if (!master_entries[i].in_use || !name_matches_filter(master_entries[i].name)) {
			continue;
		}

		create_row_button(master_entries[i].display_text, i, master_entries[i].greyed_out);
	}
}

/** Assumes the LVGL lock is already held. Records a new entry and renders it if it matches. */
static int add_master_entry(const char *name, const char *display_text, bool greyed_out)
{
	uint8_t index;
	size_t display_text_size;

	if (master_count >= UI_MAX_LIST_ITEMS) {
		return -ENOMEM;
	}

	index = master_count;
	display_text_size = sizeof(master_entries[index].display_text);

	(void)snprintf(master_entries[index].name, sizeof(master_entries[index].name), "%s", name);
	(void)snprintf(master_entries[index].display_text, display_text_size, "%s", display_text);
	master_entries[index].in_use = true;
	master_entries[index].greyed_out = greyed_out;
	master_count++;

	if (name_matches_filter(master_entries[index].name)) {
		create_row_button(master_entries[index].display_text, index, greyed_out);
	}

	return 0;
}

/* LVGL event callback; runs under ui_process()'s ambient lock. */
static void open_filter_keyboard(void)
{
	lv_textarea_set_text(filter_textarea, filter_text);
	lv_obj_remove_flag(filter_textarea, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(filter_keyboard, LV_OBJ_FLAG_HIDDEN);
	/* Hide everything the textarea/keyboard would otherwise overlap. */
	lv_obj_add_flag(list_widget, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(filter_toggle_btn, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
	lv_keyboard_set_textarea(filter_keyboard, filter_textarea);
}

static void close_filter_keyboard(void)
{
	lv_obj_add_flag(filter_textarea, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(filter_keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(list_widget, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(filter_toggle_btn, LV_OBJ_FLAG_HIDDEN);
	if (filter_enabled) {
		lv_obj_remove_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
	}
}

static void filter_toggle_event_cb(lv_event_t *e)
{
	ARG_UNUSED(e);

	filter_enabled = !filter_enabled;

	if (filter_enabled) {
		lv_label_set_text(filter_toggle_label, "Filter: On");
		lv_obj_remove_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
		open_filter_keyboard();
	} else {
		lv_label_set_text(filter_toggle_label, "Filter");
		lv_obj_add_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
		close_filter_keyboard();
		rebuild_visible_rows();
	}
}

static void filter_display_click_cb(lv_event_t *e)
{
	ARG_UNUSED(e);

	open_filter_keyboard();
}

static void filter_keyboard_event_cb(lv_event_t *e)
{
	lv_event_code_t code;
	const char *text;

	if (e == NULL) {
		return;
	}

	code = lv_event_get_code(e);

	if (code == LV_EVENT_READY) {
		text = lv_textarea_get_text(filter_textarea);

		(void)snprintf(filter_text, sizeof(filter_text), "%s", text != NULL ? text : "");

		for (size_t i = 0U; filter_text[i] != '\0'; i++) {
			filter_text[i] = to_lower_ascii(filter_text[i]);
		}

		filter_enabled = (filter_text[0] != '\0');

		lv_label_set_text(filter_toggle_label, filter_enabled ? "Filter: On" : "Filter");
		lv_label_set_text(filter_display_label,
				  filter_enabled ? filter_text : "(tap Filter to search)");

		if (!filter_enabled) {
			lv_obj_add_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
		}

		close_filter_keyboard();
		rebuild_visible_rows();
	} else if (code == LV_EVENT_CANCEL) {
		close_filter_keyboard();
	}
}

/**
 * Assumes the LVGL lock is already held. Points one hand at the given
 * position in the 60-entry direction table (see clock_dir_x/y above) and
 * pushes the new endpoint to its lv_line object.
 */
static void set_clock_hand(lv_obj_t *line, lv_point_precise_t points[2], int32_t table_index,
			   int32_t length)
{
	int32_t idx;

	if (line == NULL || points == NULL) {
		return;
	}

	/* Defensive: table_index is always caller-computed in [0, 59], but clamp anyway. */
	idx = table_index % CLOCK_TABLE_SIZE;
	if (idx < 0) {
		idx += CLOCK_TABLE_SIZE;
	}

	points[0].x = CLOCK_CENTER_X;
	points[0].y = CLOCK_CENTER_Y;
	points[1].x = CLOCK_CENTER_X + (clock_dir_x[idx] * length) / 1000;
	points[1].y = CLOCK_CENTER_Y + (clock_dir_y[idx] * length) / 1000;

	lv_line_set_points(line, points, 2);
}

/** Returns 0-11 for a 3-letter month abbreviation ("Jan".."Dec"), or -1 if unrecognized. */
static int clock_month_from_abbrev(const char *abbrev)
{
	static const char *const names[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	};

	if (abbrev == NULL) {
		return -1;
	}

	for (int i = 0; i < 12; i++) {
		if (strncmp(abbrev, names[i], 3) == 0) {
			return i;
		}
	}

	return -1;
}

/** Parses an len-digit field that may contain leading spaces (as in "__DATE__"'s day field). */
static int clock_parse_uint_field(const char *field, size_t len)
{
	int value = 0;
	bool any_digit = false;

	if (field == NULL) {
		return -1;
	}

	for (size_t i = 0U; i < len; i++) {
		char c = field[i];

		if (c == ' ') {
			continue;
		}
		if (c < '0' || c > '9') {
			return -1;
		}
		value = (value * 10) + (c - '0');
		any_digit = true;
	}

	return any_digit ? value : -1;
}

/** Sakamoto's algorithm: day of week for a Gregorian date, 0=Sunday..6=Saturday. */
static int clock_day_of_week(int year, int month1_12, int day)
{
	static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	int y = year;

	if (month1_12 < 1 || month1_12 > 12) {
		return -1;
	}

	if (month1_12 < 3) {
		y -= 1;
	}

	return (y + y / 4 - y / 100 + y / 400 + t[month1_12 - 1] + day) % 7;
}

/**
 * Assumes the LVGL lock is already held (called once from ui_init(), before the display
 * is first shown). Sets the RTC to this firmware's build date/time, parsed from the
 * standard __DATE__/__TIME__ preprocessor macros, so the clock starts near a real time
 * and visibly ticking instead of sitting at whatever the RTC happened to power on with.
 *
 * This intentionally overwrites the RTC's time on every boot. That means the clock will
 * be off by however long ago this firmware was built/flashed rather than showing wall
 * time, and it will discard any real time previously set via rtc_set_time() or the RTC
 * shell (CONFIG_RTC_SHELL) on the last flash. Remove this call in ui_init() if you'd
 * rather the battery-backed RTC's time persist across reflashes/reboots.
 */
static void set_rtc_to_build_time(void)
{
	static const char build_date[] = __DATE__; /* "Mmm dd yyyy" */
	static const char build_time[] = __TIME__; /* "hh:mm:ss" */
	struct rtc_time tm = {0};
	int month;
	int day;
	int year;
	int hour;
	int minute;
	int second;
	int wday;
	int err;

	if (clock_rtc_dev == NULL || !device_is_ready(clock_rtc_dev)) {
		return;
	}

	if (sizeof(build_date) != 12U || sizeof(build_time) != 9U) {
		LOG_WRN("Unexpected __DATE__/__TIME__ length; not setting RTC");
		return;
	}

	month = clock_month_from_abbrev(&build_date[0]);
	day = clock_parse_uint_field(&build_date[4], 2U);
	year = clock_parse_uint_field(&build_date[7], 4U);
	hour = clock_parse_uint_field(&build_time[0], 2U);
	minute = clock_parse_uint_field(&build_time[3], 2U);
	second = clock_parse_uint_field(&build_time[6], 2U);

	if (month < 0 || day < 1 || day > 31 || year < 1970 || year > 9999 || hour < 0 ||
	    hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
		LOG_WRN("Failed to parse build date/time (\"%s\" \"%s\"); not setting RTC",
			build_date, build_time);
		return;
	}

	wday = clock_day_of_week(year, month + 1, day);

	tm.tm_year = year - 1900;
	tm.tm_mon = month;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_wday = (wday >= 0) ? wday : -1;
	tm.tm_yday = -1;
	tm.tm_isdst = -1;
	tm.tm_nsec = 0;

	err = rtc_set_time(clock_rtc_dev, &tm);
	if (err != 0) {
		LOG_WRN("Failed to set RTC to build time (err %d)", err);
		return;
	}

	LOG_INF("RTC set to build time: %04d-%02d-%02d %02d:%02d:%02d", year, month + 1, day, hour,
		minute, second);
}

/** Assumes the LVGL lock is already held. Reads the RTC, if any, and moves the clock hands. */
static void update_clock_hands(void)
{
	struct rtc_time tm;
	int err;
	int32_t hour_idx;
	int32_t min_idx;
	int32_t sec_idx;

	if (clock_rtc_dev == NULL || !device_is_ready(clock_rtc_dev)) {
		return;
	}

	err = rtc_get_time(clock_rtc_dev, &tm);
	if (err != 0) {
		return;
	}

	if (tm.tm_hour < 0 || tm.tm_hour > 23 || tm.tm_min < 0 || tm.tm_min > 59 || tm.tm_sec < 0 ||
	    tm.tm_sec > 59) {
		/* Leave the hands at their last (or initial) position rather than guessing. */
		return;
	}

	sec_idx = tm.tm_sec;
	min_idx = tm.tm_min;
	/* 12 hours span the same 60-slot table as minutes/seconds: 60 slots / 12 h = 12 min/slot.
	 */
	hour_idx = ((tm.tm_hour % 12) * 60 + tm.tm_min) / 12;

	set_clock_hand(clock_hour_hand, clock_hour_points, hour_idx, CLOCK_HOUR_HAND_LEN);
	set_clock_hand(clock_min_hand, clock_min_points, min_idx, CLOCK_MIN_HAND_LEN);
	set_clock_hand(clock_sec_hand, clock_sec_points, sec_idx, CLOCK_SEC_HAND_LEN);
}

/* LVGL event callback; runs under ui_process()'s ambient lock. */
static void clock_screen_click_cb(lv_event_t *e)
{
	ui_clock_tapped_cb cb = clock_tapped_cb;

	ARG_UNUSED(e);

	if (cb != NULL) {
		cb();
	}
}

static void exit_button_event_cb(lv_event_t *e)
{
	ui_exit_cb cb = exit_cb;

	ARG_UNUSED(e);

	if (cb != NULL) {
		cb();
	}
}

/** Caller must NOT hold the LVGL lock; this takes it for its whole critical section. */
static void clear_rows(enum ui_screen screen, const char *title)
{
	lv_lock();

	lv_obj_clean(list_widget);
	memset(rows, 0, sizeof(rows));
	row_count = 0U;
	memset(master_entries, 0, sizeof(master_entries));
	master_count = 0U;
	filter_text[0] = '\0';
	filter_enabled = false;
	active_screen = screen;

	lv_label_set_text(list_title_label, title);
	lv_label_set_text(filter_toggle_label, "Filter");
	lv_label_set_text(filter_display_label, "");
	lv_obj_add_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(filter_textarea, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(filter_keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(list_widget, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(filter_toggle_btn, LV_OBJ_FLAG_HIDDEN);

	lv_screen_load(list_screen);

	lv_unlock();
}

int ui_init(ui_row_selected_cb on_row_selected, ui_clock_tapped_cb on_clock_tapped,
	    ui_exit_cb on_exit)
{
	int ret;
	const struct device *display_dev;
	lv_obj_t *clock_logo;

	if (on_row_selected == NULL || on_clock_tapped == NULL || on_exit == NULL) {
		return -EINVAL;
	}

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device is not ready");
		return -ENODEV;
	}

	row_selected_cb = on_row_selected;
	clock_tapped_cb = on_clock_tapped;
	exit_cb = on_exit;

	lv_lock();

	/* --- Splash screen --- */
	splash_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(splash_screen, lv_color_hex(NORDIC_COLOR_BG), 0);
	lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(splash_screen, 0, 0);

	lv_obj_t *splash_logo = lv_image_create(splash_screen);

	lv_image_set_src(splash_logo, &nordic_logo);
	lv_obj_center(splash_logo);

	/* --- Clock screen (idle / home screen) ---
	 *
	 * An analog face: 12 tick marks, then the Nordic logo scaled down and
	 * centered, then the three hands last - LVGL draws children in
	 * creation order, so the hands (created last) render on top of the
	 * logo, sweeping visibly across it, rather than being hidden behind it.
	 * Shown after the splash and whenever Exit is tapped on a list screen.
	 * Tapping anywhere on it starts a scan (via on_clock_tapped). The time
	 * comes from the board's RTC if one is fitted and has been set; see
	 * update_clock_hands().
	 */
	clock_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(clock_screen, lv_color_hex(NORDIC_COLOR_BG), 0);
	lv_obj_add_flag(clock_screen, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(clock_screen, clock_screen_click_cb, LV_EVENT_CLICKED, NULL);

	for (uint8_t tick = 0U; tick < 12U; tick++) {
		/* Every 5th table slot is one of the 12 hour positions (60 slots / 12 = 5). */
		uint8_t idx = tick * 5U;
		lv_obj_t *dot = lv_obj_create(clock_screen);
		int32_t tx = CLOCK_CENTER_X + (clock_dir_x[idx] * CLOCK_TICK_RADIUS) / 1000;
		int32_t ty = CLOCK_CENTER_Y + (clock_dir_y[idx] * CLOCK_TICK_RADIUS) / 1000;

		lv_obj_set_size(dot, 6, 6);
		lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(dot, lv_color_hex(NORDIC_COLOR_BLUE), 0);
		lv_obj_set_style_border_width(dot, 0, 0);
		lv_obj_align(dot, LV_ALIGN_TOP_LEFT, tx - 3, ty - 3);
	}

	clock_logo = lv_image_create(clock_screen);
	lv_image_set_src(clock_logo, &nordic_logo);
	lv_image_set_scale(clock_logo, 130); /* ~56x48, scaled down from the 110x95 source */
	lv_obj_align(clock_logo, LV_ALIGN_CENTER, 0, 0);

	clock_hour_hand = lv_line_create(clock_screen);
	lv_obj_set_style_line_width(clock_hour_hand, 5, 0);
	lv_obj_set_style_line_rounded(clock_hour_hand, true, 0);
	lv_obj_set_style_line_color(clock_hour_hand, lv_color_hex(NORDIC_COLOR_TEXT), 0);

	clock_min_hand = lv_line_create(clock_screen);
	lv_obj_set_style_line_width(clock_min_hand, 4, 0);
	lv_obj_set_style_line_rounded(clock_min_hand, true, 0);
	lv_obj_set_style_line_color(clock_min_hand, lv_color_hex(NORDIC_COLOR_TEXT), 0);

	/* A distinct accent color (not the tick/brand blue) so the seconds hand stands out. */
	clock_sec_hand = lv_line_create(clock_screen);
	lv_obj_set_style_line_width(clock_sec_hand, 2, 0);
	lv_obj_set_style_line_rounded(clock_sec_hand, true, 0);
	lv_obj_set_style_line_color(clock_sec_hand, lv_color_hex(CLOCK_SEC_HAND_COLOR), 0);

	/* Default to 12:00:00 until set_rtc_to_build_time()/update_clock_hands() move them. */
	set_clock_hand(clock_hour_hand, clock_hour_points, 0, CLOCK_HOUR_HAND_LEN);
	set_clock_hand(clock_min_hand, clock_min_points, 0, CLOCK_MIN_HAND_LEN);
	set_clock_hand(clock_sec_hand, clock_sec_points, 0, CLOCK_SEC_HAND_LEN);

	set_rtc_to_build_time();

	/* --- List screen (source / sink discovery) ---
	 *
	 * Layout is budgeted for the 240x240 round panel and calibrated against
	 * on-device photos of this exact build: a short, large title sits near
	 * the top of the visible circle (confirmed visible around y=12-42), a
	 * compact filter toggle + "current filter" button sit just under it, and
	 * the results list occupies a confirmed-visible band around y=110-205
	 * rather than running flush to the bottom edge, where content was
	 * confirmed to fall outside the visible circle. The list itself is a
	 * generously rounded panel (not a hard-cornered rectangle) rather than a
	 * full circular clip: clipping a scrollable column of tappable rows to a
	 * circle makes off-center rows partially unreadable and hard to tap - a
	 * bad trade for a touch list.
	 */
	list_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(list_screen, lv_color_hex(NORDIC_COLOR_BG), 0);

	list_title_label = lv_label_create(list_screen);
	lv_obj_set_style_text_font(list_title_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(list_title_label, lv_color_hex(NORDIC_COLOR_BLUE), 0);
	lv_obj_align(list_title_label, LV_ALIGN_TOP_MID, 0, 12);

	filter_toggle_btn = lv_button_create(list_screen);
	lv_obj_set_size(filter_toggle_btn, 84, 28);
	lv_obj_align(filter_toggle_btn, LV_ALIGN_TOP_MID, -46, 46);
	lv_obj_set_style_bg_color(filter_toggle_btn, lv_color_hex(NORDIC_COLOR_SURFACE), 0);
	lv_obj_set_style_bg_color(filter_toggle_btn, lv_color_hex(NORDIC_COLOR_BLUE),
				  LV_STATE_PRESSED);
	lv_obj_add_event_cb(filter_toggle_btn, filter_toggle_event_cb, LV_EVENT_CLICKED, NULL);

	filter_toggle_label = lv_label_create(filter_toggle_btn);
	lv_label_set_text(filter_toggle_label, "Filter");
	lv_obj_set_style_text_color(filter_toggle_label, lv_color_hex(NORDIC_COLOR_TEXT), 0);
	lv_obj_center(filter_toggle_label);

	exit_btn = lv_button_create(list_screen);
	lv_obj_set_size(exit_btn, 70, 28);
	lv_obj_align(exit_btn, LV_ALIGN_TOP_MID, 44, 46);
	lv_obj_set_style_bg_color(exit_btn, lv_color_hex(NORDIC_COLOR_SURFACE), 0);
	lv_obj_set_style_bg_color(exit_btn, lv_color_hex(NORDIC_COLOR_BLUE), LV_STATE_PRESSED);
	lv_obj_add_event_cb(exit_btn, exit_button_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *exit_label = lv_label_create(exit_btn);

	lv_label_set_text(exit_label, "Exit");
	lv_obj_set_style_text_color(exit_label, lv_color_hex(NORDIC_COLOR_TEXT), 0);
	lv_obj_center(exit_label);

	filter_display_btn = lv_button_create(list_screen);
	lv_obj_set_size(filter_display_btn, 170, 26);
	lv_obj_align(filter_display_btn, LV_ALIGN_TOP_MID, 0, 80);
	lv_obj_set_style_bg_color(filter_display_btn, lv_color_hex(NORDIC_COLOR_BG), 0);
	lv_obj_set_style_border_color(filter_display_btn, lv_color_hex(NORDIC_COLOR_BLUE), 0);
	lv_obj_set_style_border_width(filter_display_btn, 1, 0);
	lv_obj_add_event_cb(filter_display_btn, filter_display_click_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_flag(filter_display_btn, LV_OBJ_FLAG_HIDDEN);

	filter_display_label = lv_label_create(filter_display_btn);
	lv_obj_set_style_text_color(filter_display_label, lv_color_hex(NORDIC_COLOR_BLUE), 0);
	lv_obj_center(filter_display_label);

	/*
	 * A one-line textarea sized at 32px was too tight for the default
	 * Montserrat 14 font plus its default top/bottom padding: the content
	 * area ended up a couple of pixels taller than the box, so LVGL showed
	 * a vertical scrollbar and kept re-centering the cursor into view each
	 * blink, which read as the box "shaking". Explicit small padding plus
	 * a slightly taller box gives the single line of text enough room, and
	 * the scrollbar is turned off outright since a one-line search field
	 * never needs one (horizontal overflow still scrolls without it).
	 */
	filter_textarea = lv_textarea_create(list_screen);
	lv_textarea_set_one_line(filter_textarea, true);
	lv_textarea_set_placeholder_text(filter_textarea, "Name or NRF*...");
	lv_obj_set_size(filter_textarea, 190, 36);
	lv_obj_set_style_text_font(filter_textarea, &lv_font_montserrat_14, 0);
	lv_obj_set_style_pad_top(filter_textarea, 3, 0);
	lv_obj_set_style_pad_bottom(filter_textarea, 3, 0);
	lv_obj_set_scrollbar_mode(filter_textarea, LV_SCROLLBAR_MODE_OFF);
	lv_obj_align(filter_textarea, LV_ALIGN_TOP_MID, 0, 46);
	lv_obj_add_flag(filter_textarea, LV_OBJ_FLAG_HIDDEN);

	/*
	 * On-device testing on the actual round panel showed that a
	 * bottom-aligned, screen-height keyboard has its last row (the one
	 * carrying the OK/checkmark and close/X keys) fall outside the
	 * visible circular area - only the top 3 of its 4 rows were visible.
	 * The letter rows immediately above (roughly y=90 to y=202 at full
	 * 240px width) rendered perfectly, so the keyboard is now placed
	 * entirely inside that confirmed-visible band instead of flush
	 * against the bottom edge, which brings the OK/close row into view
	 * too.
	 */
	filter_keyboard = lv_keyboard_create(list_screen);
	lv_obj_set_size(filter_keyboard, 220, 120);
	lv_obj_align(filter_keyboard, LV_ALIGN_TOP_MID, 0, 82);
	lv_keyboard_set_textarea(filter_keyboard, filter_textarea);
	lv_obj_add_event_cb(filter_keyboard, filter_keyboard_event_cb, LV_EVENT_ALL, NULL);
	lv_obj_add_flag(filter_keyboard, LV_OBJ_FLAG_HIDDEN);

	/*
	 * Sized and positioned to sit entirely within the same confirmed-visible
	 * band as the keyboard above (see the comment on filter_keyboard):
	 * roughly y=110 to y=205 at this width, rather than running flush to the
	 * bottom edge where content was confirmed to disappear behind the bezel.
	 */
	list_widget = lv_list_create(list_screen);
	lv_obj_set_size(list_widget, 200, 95);
	lv_obj_align(list_widget, LV_ALIGN_TOP_MID, 0, 110);
	lv_obj_set_style_radius(list_widget, 24, 0);
	lv_obj_set_style_clip_corner(list_widget, true, 0);
	lv_obj_set_style_bg_color(list_widget, lv_color_hex(NORDIC_COLOR_SURFACE), 0);
	lv_obj_set_style_pad_row(list_widget, 4, 0);

	/* --- Status screen --- */
	status_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(status_screen, lv_color_hex(NORDIC_COLOR_BG), 0);

	status_title_label = lv_label_create(status_screen);
	lv_obj_set_style_text_font(status_title_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(status_title_label, lv_color_hex(NORDIC_COLOR_BLUE), 0);
	lv_obj_align(status_title_label, LV_ALIGN_CENTER, 0, -20);

	status_detail_label = lv_label_create(status_screen);
	lv_label_set_long_mode(status_detail_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(status_detail_label, 180);
	lv_obj_set_style_text_align(status_detail_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(status_detail_label, lv_color_hex(NORDIC_COLOR_TEXT), 0);
	lv_obj_align(status_detail_label, LV_ALIGN_CENTER, 0, 20);

	lv_screen_load(splash_screen);

	lv_unlock();

	ret = display_blanking_off(display_dev);
	if (ret < 0 && ret != -ENOSYS) {
		LOG_ERR("Failed to turn off display blanking (err %d)", ret);
		return ret;
	}

	return 0;
}

void ui_show_splash(void)
{
	lv_lock();
	lv_screen_load(splash_screen);
	lv_unlock();
}

void ui_show_clock(void)
{
	lv_lock();
	active_screen = UI_SCREEN_CLOCK;
	update_clock_hands();
	clock_last_update_ms = k_uptime_get();
	lv_screen_load(clock_screen);
	lv_unlock();
}

void ui_show_source_list(void)
{
	clear_rows(UI_SCREEN_SOURCE_LIST, "Sources");
}

void ui_show_sink_list(void)
{
	clear_rows(UI_SCREEN_SINK_LIST, "Sinks");
}

int ui_add_source_row(const char *name, uint32_t broadcast_id)
{
	char safe_name[UI_FILTER_NAME_LEN];
	char text[64];
	int ret;

	if (name == NULL) {
		return -EINVAL;
	}

	(void)snprintf(safe_name, sizeof(safe_name), "%s", name[0] != '\0' ? name : "(unnamed)");
	/* broadcast_id is a 24-bit BLE broadcast ID; mask it so %06X can never need more than
	 * 6 hex digits, which also lets the compiler prove snprintf() can't truncate here.
	 */
	(void)snprintf(text, sizeof(text), "%s (0x%06X)", safe_name, broadcast_id & 0xFFFFFFU);

	lv_lock();
	ret = add_master_entry(safe_name, text, false);
	lv_unlock();

	if (ret != 0) {
		LOG_WRN("List is full, dropping source: %s", safe_name);
	}

	return ret;
}

int ui_add_sink_row(const char *name)
{
	char safe_name[UI_FILTER_NAME_LEN];
	int ret;

	if (name == NULL) {
		return -EINVAL;
	}

	(void)snprintf(safe_name, sizeof(safe_name), "%s", name[0] != '\0' ? name : "(unnamed)");

	lv_lock();
	ret = add_master_entry(safe_name, safe_name, false);
	lv_unlock();

	if (ret != 0) {
		LOG_WRN("List is full, dropping sink: %s", safe_name);
	}

	return ret;
}

void ui_set_sink_configured(uint8_t index, bool configured)
{
	lv_lock();

	if (index < master_count && master_entries[index].in_use) {
		master_entries[index].greyed_out = configured;
		rebuild_visible_rows();
	}

	lv_unlock();
}

void ui_show_status(const char *title, const char *detail)
{
	lv_lock();

	active_screen = UI_SCREEN_STATUS;
	lv_label_set_text(status_title_label, title != NULL ? title : "");
	lv_label_set_text(status_detail_label, detail != NULL ? detail : "");
	lv_screen_load(status_screen);

	lv_unlock();
}

void ui_process(void)
{
	int64_t now;

	/* Take the LVGL lock even though this is the "owner" thread: row_click_event_cb()
	 * and the filter event callbacks run from within lv_timer_handler() and touch
	 * state that is also written by ui_add_*_row()/ui_show_*()/etc. from other threads.
	 */
	lv_lock();

	lv_timer_handler();

	if (active_screen == UI_SCREEN_CLOCK) {
		now = k_uptime_get();
		if (now - clock_last_update_ms >= 1000) {
			update_clock_hands();
			clock_last_update_ms = now;
		}
	}

	lv_unlock();
}
