#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("Audio module")

target_sources(app PRIVATE src/main.c src/fakes.c src/decoder_test.c)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/include)

zephyr_library_include_directories(${ZEPHYR_NRF_MODULE_DIR}/subsys/a_mod)
zephyr_library_include_directories(${ZEPHYR_NRF_MODULE_DIR}/subsys/audio/a_mod/lc3_t2/decoder)
zephyr_library_include_directories(${ZEPHYR_NRF_MODULE_DIR}/../nrfxlib/lc3/codec/inc)
zephyr_library_include_directories(${ZEPHYR_NRF_MODULE_DIR}/../nrfxlib/lc3/platform/os/inc)
zephyr_library_include_directories(${ZEPHYR_NRF_MODULE_DIR}/../nrfxlib/lc3/platform/os/baremetal/inc)
