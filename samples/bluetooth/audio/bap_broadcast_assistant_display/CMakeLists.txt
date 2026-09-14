# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bap_broadcast_assistant_display)

target_sources(app PRIVATE
  src/main.c
  src/ble_scan_ctrl.c
  src/ui.c
  src/nordic_logo.c
)
