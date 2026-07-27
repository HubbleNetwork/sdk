/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_HUBBLE_H
#define INCLUDE_HUBBLE_HUBBLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_HUBBLE_SAT_NETWORK
#include <hubble/sat.h>
#endif /* CONFIG_HUBBLE_SAT_NETWORK */

#ifdef CONFIG_HUBBLE_BLE_NETWORK
#include <hubble/ble.h>
#endif /* CONFIG_HUBBLE_BLE_NETWORK */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble Device SDK APIs
 * @defgroup hubble_api Hubble Device SDK APIs
 * @{
 */

/**
 * @brief Initializes the Hubble Device SDK.
 *
 * Calling this function is essential before using any other SDK APIs.
 *
 * The interpretation of the `initial_time` parameter depends on the configured
 * counter source:
 *
 * **Unix time mode (CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME):**
 *   - `initial_time` is the time in milliseconds since Unix epoch
 *   - Value of 0 is invalid and will return an error
 *   - Time can be updated later via hubble_time_set()
 *
 * **Device uptime mode (CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME):**
 *   - `initial_time` is the initial counter value
 *   - Value of 0 starts the counter at epoch 0 (valid)
 *   - The counter increments based on uptime from this initial value
 *   - Useful for resuming from a known state after reboot
 *
 * @code
 * // Unix time mode example
 * uint64_t unix_time_ms = 1705881600000; // Current Unix Epoch time
 * int ret = hubble_init(unix_time_ms, master_key);
 *
 * // Device uptime mode example (start at 0)
 * int ret = hubble_init(0, master_key);
 *
 * // Device uptime mode example (resume from saved counter)
 * uint64_t saved_counter = load_from_flash();
 * int ret = hubble_init(saved_counter, master_key);
 * @endcode
 *
 * @param initial_time For Unix time mode: Unix time in milliseconds since
 *                     epoch. For device uptime mode: Initial counter value
 *                     (0 = start at 0).
 * @param key An opaque pointer to the master key buffer. The SDK
 *            stores this pointer directly and does not copy the key
 *            data. The caller must ensure the buffer remains valid
 *            for the lifetime of SDK usage (do not use stack or
 *            temporary buffers). The key size must match
 *            CONFIG_HUBBLE_NETWORK_KEY_256 or CONFIG_HUBBLE_NETWORK_KEY_128.
 *            If NULL, must be set with hubble_key_set before getting advertisements.
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_init(uint64_t initial_time, const void *key);

/**
 * @brief Sets the current Unix time in the Hubble Device SDK.
 *
 * @param unix_time The Unix Epoch time in milliseconds since the Unix epoch
 *                  (January 1, 1970).
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_time_set(uint64_t unix_time);

/**
 * @brief Gets the current time.
 *
 * Gets the time in milliseconds since the Unix epoch (January 1, 1970).
 *
 * @return
 *          - 0 if time is not set.
 *          - current time since the Unix epoch.
 */
uint64_t hubble_time_get(void);

/**
 * @brief Sets the encryption key for advertisement data creation.
 *
 * @param key An opaque pointer to the master key buffer. The SDK
 *            stores this pointer directly and does not copy the key
 *            data. The caller must ensure the buffer remains valid
 *            for the lifetime of SDK usage. The key size must match
 *            CONFIG_HUBBLE_NETWORK_KEY_256 or CONFIG_HUBBLE_NETWORK_KEY_128.
 *
 * @return
 *         - 0 on success.
 *         - Non-zero on failure.
 */
int hubble_key_set(const void *key);

/**
 * @brief Get the current time counter value.
 *
 * Returns the time counter based on the configured counter source:
 * - Device uptime: Uses uptime-derived counter with initial offset, wrapping
 *                  at HUBBLE_EID_POOL_SIZE (128) to produce values in [0, pool_size-1]
 * - Unix time: Uses Unix time divided by rotation period (no wrapping)
 *
 * @param counter Pointer to store the time counter value
 * @return 0 on success, negative error code on failure (Unix time mode only, if time not set)
 */
int hubble_counter_get(uint32_t *counter);

/**
 * @brief Get the Hubble Device Configuration Vector (HDCV) for this build.
 *
 * Returns a compact, fixed-format string that describes the crypto,
 * EID-rotation and network configuration compiled into the SDK, for example
 * `HDCV:1.0/E:256/CS:UT/RP:S86400/N:T/TV:1`. The string is derived entirely
 * from build-time options, so it can be logged, stored, or reported to identify
 * how a device is configured.
 *
 * @return Pointer to a static, NUL-terminated HDCV string. Never NULL.
 */
const char *hubble_config_vector_get(void);

/**
 * @brief Check whether a Hubble Device Configuration Vector (HDCV) string
 * matches this build's configuration.
 *
 * Compares @p hdcv against the Hubble Device Configuration Vector for this
 * build (the string returned by @ref hubble_config_vector_get). Use it to
 * confirm that a device is configured as expected — for example, that the
 * vector a provisioning tool or the Cloud recorded still matches the firmware.
 *
 * @param hdcv NUL-terminated HDCV string to check.
 *
 * @return
 *          - true if @p hdcv is non-NULL and equals this build's HDCV string.
 *          - false otherwise (including when @p hdcv is NULL).
 */
bool hubble_config_vector_check(const char *hdcv);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_HUBBLE_H */
