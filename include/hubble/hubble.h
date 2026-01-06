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
 * @brief Initializes the Hubble.
 *
 * Calling this function is essential before using any other SDK APIs.
 *
 * @code
 * uint64_t current_utc_time = 1633072800000; // Example UTC time in milliseconds
 * static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE] = {...};
 * int ret = hubble_init(current_utc_time, master_key);
 * @endcode
 *
 * @param utc_time The UTC time in milliseconds since the Unix epoch (January 1, 1970).
 *                 Set to 0 to set later via hubble_utc_set
 * @param key An opaque pointer to the key. If NULL, must be set with hubble_key_set
 *            before getting advertisements.
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_init(uint64_t utc_time, const void *key);

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
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_HUBBLE_H */
