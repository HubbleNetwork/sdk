/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_HUBBLE_H
#define INCLUDE_HUBBLE_HUBBLE_H

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
 * @brief Hubble Network SDK APIs
 * @defgroup hubble_api Hubble Network APIs
 * @{
 */

/**
 * @brief Initializes the Hubble SDK.
 *
 * Calling this function is essential before using any other SDK APIs.
 *
 * The interpretation of the `initial_time` parameter depends on the configured
 * EID generation mode:
 *
 * **UTC-based mode (CONFIG_HUBBLE_EID_UTC_BASED):**
 *   - `initial_time` is the UTC time in milliseconds since Unix epoch
 *   - Value of 0 is invalid and will return an error
 *   - Can be updated later via hubble_utc_set()
 *
 * **Counter-based mode (CONFIG_HUBBLE_EID_COUNTER_BASED):**
 *   - `initial_time` is the initial EID counter value
 *   - Value of 0 starts the counter at epoch 0 (valid)
 *   - The counter increments based on uptime from this initial value
 *   - Useful for resuming from a known state after reboot
 *
 * @code
 * // UTC-based mode example
 * uint64_t utc_ms = 1705881600000; // Current UTC time
 * int ret = hubble_init(utc_ms, master_key);
 *
 * // Counter-based mode example (start at 0)
 * int ret = hubble_init(0, master_key);
 *
 * // Counter-based mode example (resume from saved counter)
 * uint64_t saved_counter = load_from_flash();
 * int ret = hubble_init(saved_counter, master_key);
 * @endcode
 *
 * @param initial_time For UTC mode: UTC time in milliseconds since epoch.
 *                     For Counter mode: Initial counter value (0 = start at 0).
 * @param key An opaque pointer to the master key. If NULL, must be set with
 *            hubble_key_set before getting advertisements.
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_init(uint64_t initial_time, const void *key);

/**
 * @brief Sets the current UTC time in the Hubble SDK.
 *
 * @param utc_time The UTC time in milliseconds since the Unix epoch (January 1, 1970).
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_utc_set(uint64_t utc_time);

/**
 * @brief Sets the encryption key for advertisement data creation.
 *
 * @param key An opaque pointer to the key.
 *
 * @return
 *         - 0 on success.
 *         - Non-zero on failure.
 */
int hubble_key_set(const void *key);

/**
 * @brief Gets the current EID epoch counter value.
 *
 * Returns the current epoch counter used for EID generation. The value
 * interpretation depends on the configured EID mode (UTC-based or counter-based).
 *
 * In counter-based mode, this value can be saved to persistent storage and
 * passed to hubble_init() on the next boot to maintain EID continuity.
 *
 * @return Current EID epoch counter value.
 */
uint32_t hubble_eid_counter_get(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_HUBBLE_H */
