# Broadcast Assistant with round-display touch UI

Implements the LE Audio **Broadcast Assistant** role (BAP/BASS client) on a
Seeed Studio XIAO nRF54LM20A fitted with the Seeed Studio XIAO Round Display
shield (GC9A01 LCD + CHSC6X touch controller).

> Terminology note: the sample scans for and configures **Scan Delegators**
> (the BASS *server* role, found on Broadcast Sink devices) — that server
> role is what is usually meant by "Scan Delegator". The device you flash
> this sample onto plays the complementary client role, **Broadcast
> Assistant**, which is what actually does the scanning, and the tapping UI
> described in the request. This sample implements that client role and
> talks to Scan Delegators running on other, unmodified LE Audio sink
> devices.

## Flow

0. Shows a startup splash screen for ~2 seconds, then the idle **clock
   screen**: an analog face (12 tick marks, hour/minute/second hands) with
   the Nordic logo centered over the hands' pivot. Tap anywhere on it to
   start scanning.
1. Scans for Broadcast Sources (LE Audio broadcasters) and lists them. Tap
   "Filter" under the title to search by name: it opens an on-screen
   keyboard, type a term and press the keyboard's enter/OK key to apply it
   (case-insensitive), or Cancel to back out without changing the filter.
   A term with no `*` is a plain "contains" search; add `*` for wildcard
   matching anchored to the whole name — `NRF*` matches names starting
   with "NRF", `*CASTER` matches names ending with "caster", `NRF*ER`
   matches both ends at once. Tap "Filter" again to turn filtering off
   (the typed term is remembered), or tap the filter-term button next to
   it to edit the term.
2. Tap a source: periodic-advertising sync to it starts in the background,
   and the UI switches to scanning for Broadcast Sinks (BASS + PACS capable
   connectable devices) — filtering works the same way on this screen.
3. Tap a sink: connects to it, discovers its Broadcast Audio Scan Service,
   and once the selected source's BASE has been parsed, adds the source to
   it so it starts receiving the broadcast — it shows up greyed out and
   stays connected. Sink scanning keeps running, so more sinks (up to
   `BLE_SCAN_CTRL_MAX_SINK_CONN`, 4) can be picked the same way.
4. Tap a greyed-out sink to remove the source from it over that same
   connection — no reconnect — so it goes back to being selectable. Tap it
   again to add the source back. Either way the sink stays connected
   throughout. Note: on at least one piece of test hardware, removing the
   source did not actually stop that sink from streaming audio - see
   "Multiple sinks, each toggled independently" below.

At any point during 1-4, tap **Exit** (next to Filter) to stop scanning,
disconnect every connected sink, and return to the clock screen.

## Layout

- [src/ble_scan_ctrl.h](src/ble_scan_ctrl.h) / [src/ble_scan_ctrl.c](src/ble_scan_ctrl.c) —
  all Bluetooth LE Audio Broadcast Assistant logic. No display code.
- [src/ui.h](src/ui.h) / [src/ui.c](src/ui.c) — LVGL touch UI for the round
  display. No Bluetooth code.
- [src/main.c](src/main.c) — wires the two modules together.

## Building

```
west build -b xiao_nrf54lm20a/nrf54lm20a/cpuapp -- -DSHIELD=seeed_xiao_round_display
west flash
```

### Bluetooth controller

This sample runs on the Nordic **SoftDevice Controller** (`CONFIG_BT_LL_SOFTDEVICE=y`
in [prj.conf](prj.conf)), not Zephyr's open-source `BT_LL_SW_SPLIT` controller.
As shipped, the XIAO nRF54LM20A board devicetree enables *both* controllers'
HCI nodes at once (`&bt_hci_sdc` from the shared SoC devicetree, plus
`&bt_hci_controller` from the board's own files), which makes Kconfig default
both `CONFIG_BT_LL_SOFTDEVICE` and `CONFIG_BT_LL_SW_SPLIT` to `y` and link both
controllers into the same image — a duplicate-symbol build failure.
[boards/xiao_nrf54lm20a_nrf54lm20a_cpuapp.overlay](boards/xiao_nrf54lm20a_nrf54lm20a_cpuapp.overlay)
disables `&bt_hci_controller` and repoints the `zephyr,bt-hci` chosen node at
`&bt_hci_sdc` so only the SoftDevice Controller is built.

### Stack size and hardware stack protection

On-device testing hit a `USAGE FAULT: Illegal load of EXC_RETURN into PC`
while tapping a row during an active scan — the classic signature of a
stack overflow silently corrupting an exception frame. Comparing against
Nordic's own reference boards for the same SoC (`nrf54lm20dk`,
`nrf54l15dk`) turned up the same kind of board-support gap as the
Bluetooth controller one above: both enable `CONFIG_HW_STACK_PROTECTION`
by default in their board `Kconfig.defconfig` (`default
ARCH_HAS_STACK_PROTECTION`), but the XIAO board port never added that
default, so an overflow on this board corrupts memory instead of being
caught cleanly. `prj.conf` now sets `CONFIG_HW_STACK_PROTECTION=y`
directly (so any future overflow reports clearly instead of crashing this
way) and raises `CONFIG_MAIN_STACK_SIZE` (4096 -> 8192) and
`CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE` (2048 -> 4096), since the main thread
is where LVGL rendering, row-tap handling, and the Broadcast Assistant
calls it triggers all run.

### Periodic advertising sync options

Selecting a source used to fail with `bt_le_per_adv_sync_create() = -ENOTSUP`
on-device. Zephyr's host implementation (`subsys/bluetooth/host/scan.c`)
rejects `BT_LE_PER_ADV_SYNC_OPT_FILTER_DUPLICATE` outright unless the
controller reports the periodic-advertising ADI feature bit
(`BT_FEAT_LE_PER_ADV_ADI_SUPP`) — this SoC's current SoftDevice Controller
firmware doesn't. [src/ble_scan_ctrl.c](src/ble_scan_ctrl.c) no longer
requests that option; it was only a controller-side duplicate filter, and
`pa_recv_cb()` already ignores further PA reports once the source's BASE
has been parsed, so nothing relies on it.

## Colors

The UI uses Nordic Semiconductor's actual brand colors, sampled from the
computed CSS styles of nordicsemi.com rather than guessed from memory:
accent blue `#00A9CE` (titles, active states, links), body text `#333F48`,
page background `#FFFFFF`, and a light `#F6F6F6` surface color for panels
and buttons. See the `NORDIC_COLOR_*` defines near the top of
[src/ui.c](src/ui.c).

## Fitting the round display

Layout is calibrated against an on-device photo of this exact build, not
just guessed from the panel's nominal 240x240 resolution:

- Every screen's title uses a larger font (Montserrat 24) and short text
  ("Sources" / "Sinks") positioned near the top of the visible circle
  (confirmed visible around y=12-42).
- The photo showed that content run flush to the bottom edge — specifically
  the on-screen keyboard's bottom row, carrying the OK/checkmark and
  close/X keys — fell outside the visible circular area and simply wasn't
  visible, while the rows directly above it (roughly y=90-202 at full
  240px width) rendered fine. The keyboard and the results list are now
  both positioned entirely inside that confirmed-visible band (list:
  y=110-205; keyboard: y=82-202) instead of running to the bottom edge, so
  every key — OK and Cancel included — is visible and reachable.
- The results list is a generously rounded panel (24px corner radius, not
  a hard-cornered rectangle) rather than a full circular clip: clipping a
  scrollable column of tappable rows to a circle makes off-center rows
  partially unreadable and hard to tap, which is a bad trade for a touch
  list.
- True per-character curved/arc title text ("a text circle") still isn't
  implemented, for the same reason as before — LVGL v9's per-object
  rotation/pivot styles haven't been exercised anywhere else in this
  project, so it's the kind of thing best attempted with a build in hand
  rather than guessed at here. Happy to take a pass at it now that the rest
  of the layout has been confirmed against real hardware.
- Up to `BLE_SCAN_CTRL_MAX_RESULTS` (8) sources/sinks are tracked per scan
  round; further results are logged and dropped rather than overflowing any
  buffer.

## Nordic logo asset

[src/nordic_logo.c](src/nordic_logo.c) / [src/nordic_logo.h](src/nordic_logo.h)
hold the actual Nordic Semiconductor logo as an `lv_image_dsc_t` (RGB565,
110x95, flattened onto white), used on both the splash and clock screens.
It was generated from `nrf/scripts/shell/ble_console/docs/nordic_semi_logo.png`,
which already ships inside this NCS checkout — not fabricated or
hand-approximated. If that source PNG ever changes, regenerate with a
Pillow script following the same steps: resize, composite onto white,
pack each pixel as RGB565 (5-6-5 bits, low byte first), and emit as a
`static const uint8_t[]` wrapped in the `lv_image_dsc_t` shown in
`nordic_logo.c`.

## Clock screen and RTC

The idle clock screen is analog, reading the time from the board's
real-time clock: the seeed_xiao_round_display shield wires up a
battery-backed NXP PCF8563 RTC chip over I2C and exposes it via the
devicetree `rtc` alias, and [src/ui.c](src/ui.c) reads it once a second
with Zephyr's `rtc_get_time()` API and repositions the three hands (see
`update_clock_hands()`). This chip has no network or BLE time source of
its own — nothing in this sample sets its time — so the hands will sit at
12:00:00 (or wherever the RTC happens to power on) until something else
sets it, for example via `rtc_set_time()` or the RTC shell
(`CONFIG_RTC_SHELL`). This is a hardware/firmware-scope limitation, not a
bug: the clock is accurate once the RTC itself has been set.

Hand angles come from a precomputed 60-entry integer direction table
(`clock_dir_x`/`clock_dir_y` in `src/ui.c`) rather than calling `sinf()`/
`cosf()` at runtime — one table covers all three hands (seconds and
minutes index it directly 0-59; hours index it at 12 minutes per slot,
`(hour%12)*5 + minute/12`), so the clock face has no dependency on libm
being linked for this target, just integer multiply/divide.

## Multiple sinks, kept connected, toggled in place

A sink connects once and stays connected: tapping it again to stop or
restart the broadcast never disconnects and reconnects, it just adds or
removes the source from that sink's Broadcast Audio Scan Service over the
connection already open to it. Several sinks can be connected and toggled
independently at the same time, up to `BLE_SCAN_CTRL_MAX_SINK_CONN` (4).
This lives entirely in [src/ble_scan_ctrl.c](src/ble_scan_ctrl.c):

- **`struct sink_ctx`**, one per connected sink, replaces what used to be a
  single `sink_conn`/`bass_discovered` pair shared by whichever one sink
  was currently being connected/configured/disconnected. Each tracks its
  own connection, BASS-discovered state, whether the source is currently
  added to it, and a `busy` flag marking that some step (connect/discover/
  add/remove) is in flight for it.
- **`ble_scan_ctrl_select_sink()`** looks up whether the tapped sink already
  has a `sink_ctx` and picks one of three actions: connect it for the first
  time (no existing ctx), add the source to it (ctx exists, not currently
  configured), or remove the source from it (ctx exists, currently
  configured) — all through the same function, matching the single "tap to
  toggle" gesture in the UI. Every outcome, including of a first-time
  connect, arrives later through a `ble_scan_ctrl_cb` callback keyed by sink
  index (`sink_connected`, `source_added`, `source_removed`,
  `sink_disconnected`, or `op_failed`), not through this function's return
  value, which only reports whether the step could be started.
- **Removing a source needs its BASS Source ID**, which is never handed
  back directly (`bt_bap_broadcast_assistant_add_src()`'s callback only
  reports success/failure). Right after a source is added,
  `start_learn_src_id()` reads back the sink's receive states
  (`bt_bap_broadcast_assistant_read_recv_state()`) looking for the one
  matching the selected source's broadcast ID, and caches its Source ID on
  the `sink_ctx` — so by the time the user taps to remove it, the ID is
  already known and removal is a single direct
  `bt_bap_broadcast_assistant_rem_src()` call, no extra round trip. If the
  ID was never learned (e.g. a very fast tap), removal falls back to
  learning it first.
- **Failures don't cascade.** A connect/security/discover failure on a
  fresh sink disconnects and forgets just that one sink (`abandon_ctx()`);
  an add/remove failure on an already-connected sink leaves it exactly as
  it was (still, or not yet, configured) so the next tap can just retry —
  neither ever touches any other sink. Source-level problems (source never
  found, or its periodic advertising sync lost unexpectedly — see
  `pa_term_cb()`) are the only thing that still restarts the whole demo,
  since those mean the source itself is gone.
- **`ble_scan_ctrl_reset()`** (source re-selection, or Exit) disconnects
  every currently-connected sink at once, marking each
  `intentional_disconnect` first so the resulting disconnect events aren't
  reported as unexpected sink loss.

### Removing a source does not always stop the sink

On at least one piece of test hardware, acknowledging the Remove Source
operation was not enough on its own: the sink's BASS server ack'd the
removal (`bap_rem_src_cb()` reported success) yet the sink kept streaming
the BIS regardless. Disconnecting it afterwards to force a stop was tried,
but made things worse rather than better: that sink then would not accept
a new connection at all - `bt_conn_le_create()` kept timing out - until it
was power-cycled. Since disconnecting bought nothing (the sink not stopping
is outside anything this Broadcast Assistant client can control - its role
is limited to what it writes to the Broadcast Audio Scan Control Point) and
actively made the sink unreachable, `finish_remove()` in
[src/ble_scan_ctrl.c](src/ble_scan_ctrl.c) leaves the connection alone
either way: it only updates `sink_ctx` and reports `source_removed` /
`op_failed`, nothing more. Whether the sink's own firmware actually stops
its audio pipeline on Remove Source is up to that firmware, not this
sample.

`CONFIG_BT_MAX_CONN` and `CONFIG_BT_MAX_PAIRED` in [prj.conf](prj.conf) are
raised to 4 to match — both default to 1, which only ever fit one connected/
bonded sink at a time; observed on hardware before this was raised: bonding
a second sink while a first bond was still held failed synchronously with
`-ENOMEM`.

The UI side of the toggle is in [src/ui.c](src/ui.c): a greyed-out row is
still tappable (`style_row_button()` applies the grey colors without
`LV_STATE_DISABLED`, which would also make LVGL drop its press/click events
outright), and `ui_set_sink_configured()` flips a row's greyed-out state in
place without rebuilding the list, so toggling doesn't disturb the other
rows or scroll position.

### Scanning vs. connecting to a new sink

Scanning and *initiating a new connection* can't run at the same time on
this controller: on-device testing hit `bt_conn_le_create() = -EAGAIN`
immediately when trying to connect a second sink while still scanning for
more. Zephyr's host enforces this itself
(`conn_le_create_common_checks()` in `subsys/bluetooth/host/conn.c`
rejects it with `-EAGAIN` unless the controller's LE Supported States
report scanning and initiating as a supported concurrent combination,
which this SoftDevice Controller build doesn't) — it isn't something this
sample could opt out of.

Toggling an *already-connected* sink is unaffected: adding/removing the
source is just a GATT write over that sink's existing connection, and
running that alongside an active scan is a normal, widely-supported
combination. Only bringing up a brand new connection conflicts with
scanning. So `ble_scan_ctrl_select_sink()` only pauses sink scanning
around a fresh `bt_conn_le_create()` call, and resumes it (a plain
`bt_le_scan_start()`, not `ble_scan_ctrl_start_sink_scan()` - `found_sinks[]`
is left untouched) from `connected_cb()` as soon as that connection attempt
resolves, one way or the other. `resume_sink_scan_if_needed()` in
[src/ble_scan_ctrl.c](src/ble_scan_ctrl.c) also checks that sink scanning is
still what should be happening (not superseded by an exit/reset in the
meantime) before restarting it.

## Notes / simplifications

- No bonding/whitelist persistence: every run starts a fresh scan.
- The on-screen keyboard used for the name filter is inherently cramped on a
  1.28" round display; this is a hardware form-factor constraint, not
  something the software can fully solve.
