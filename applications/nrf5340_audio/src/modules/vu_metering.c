/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "vu_metering.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(vu_metering, CONFIG_MODULE_VU_METERING_LOG_LEVEL);

/*! Number of colour levels.*/
#define VU_COLOUR_LEVELS (20)
/*! Number of colour levels per dB. */
#define VU_COLOURS_PER_DB (4)

/*! UV metering private context. */
struct vu_context {
    /*! Last volume calculated in dB. */
    double volume_db;

    /*1 last level calculated in the range 0 to VU_COLOUR_LEVELS. */
    int level;

    /*! Intensity of red in the display. */
    uint8_t r;

    /*! Intensity of green in the display. */
    uint8_t g;

    /*! Intensity of blue in the display. */
    uint8_t b;
};

int vu_meter_rgb_get(struct vu_context_private *vu_ctx_private, int *r, uint8_t *g, uint8_t *b)
{
    int ret;
    struct vu_context vu_ctx;
 
    if (vu_ctx_private == NULL || r == NULL || g == NULL || b == NULL){
        LOG_ERR("Incorrect input parameter");
        return -EINVAL;
    }

    vu_ctx = (struct vu_context *)vu_ctx_private;

    *r = vu_ctx->r;
    *g = vu_ctx->g;
    *b = vu_ctx->b;

    return 0;
}

int vu_meter_level(struct vu_context_private *vu_ctx_private, int *samples, size_t sample_count)
{
    int ret;
    struct vu_context vu_ctx;
    double sum_powers = 0;
    double power;
    double volume_db;
    uint8_t level, level_diff;

    if (vu_ctx_private == NULL || samples == NULL || sample_count == 0){
        LOG_ERR("Incorrect input parameter");
        return -EINVAL;
    }

    vu_ctx = (struct vu_context *)vu_ctx_private;

    for(size_t i = 0; i < sample_count; i++) {
        sum_powers += pow((double)(samples[i]), 2.0);
    }

    power = sqrt(sum_powers) / sample_count;

    vu_ctx->volume_db = 20* log10(power);

    volume_db = vu_ctx->volume_db / VU_COLOURS_PER_DB;

    level = MAX(volume_db, VU_COLOUR_LEVELS);
    level_diff = vu_ctx->level - level;

    if (level_diff < 0) {
        vu_ctx->r -= level_diff;
        vu_ctx->g += level_diff;
        vu_ctx->b += level_diff;
    } else {
        vu_ctx->r += level_diff;
        vu_ctx->g -= level_diff;
        vu_ctx->b -= level_diff;       
    }

    return 0;
}