/*
 * Copyright(c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _VU_METERING_H_
#define _VU_METERING_H_

/**
 * @file
 * @defgroup vu metering within the Bluetooth LE audio modules
 * @{
 * @brief VU metering module for LE Audio applications.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Module's private handle.
 */
struct vu_context_private;

/**
 * @brief Calculate the new level and RGB output for the VU meter.
 *
 * @param vu_ctx_private  [in/out]  Pointer to the private context for the VU metering module.
 * @param samples         [in]      Pointer to the samples buffer.
 * @param sample_count    [in]      Number of samples in the buffer.
 *
 * @return 0 if successful, error otherwise.
 */
int vu_meter_level(struct vu_context_private *vu_ctx_private, int *samples, size_t sample_count)

/**
 * @brief Calculate the new level and RGB output for the VU meter.
 *
 * @param vu_ctx_private  [in]   Pointer to the private context for the VU metering module.
 * @param r               [out]  Pointer to red level.
 * @param g               [out]  Pointer to green level.
 * @param b               [out]  Pointer to blue level.
 *
 * @return 0 if successful, error otherwise.
 */
int vu_meter_rgb_get(struct vu_context_private *vu_ctx_private, uint8_t *r, uint8_t *g,  uint8_t *b)

/**
 * @}
 */

#endif /* _VU_METERING_H_ */
