/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_BLE_H
#define INCLUDE_HUBBLE_BLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble BLE Network Function APIs
 * @defgroup hubble_ble_api BLE Network Function APIs
 * @since 0.1
 * @version 0.1
 * @{
 */

/**
 * Indicates the required length of the encryption key (256 bits) for Hubble BLE Network.
 */
#define HUBBLE_BLE_KEY_LEN CONFIG_HUBBLE_KEY_SIZE

/**
 * @brief Hubble BLE Network UUID
 *
 * This is the UUID should be listed in the services list.
 */
#define HUBBLE_BLE_UUID    0xFCA6

/**
 * @brief Initializes the Hubble function with the given UTC time.
 *
 * Calling this function is essential before using @ref hubble_ble_advertise_get
 *
 * @code
 * uint64_t current_utc_time = 1633072800000; // Example UTC time in milliseconds
 * int ret = hubble_ble_init(current_utc_time);
 * @endcode
 *
 * @param utc_time The UTC time in milliseconds since the Unix epoch (January 1, 1970).
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ble_init(uint64_t utc_time);

/**
 * @brief Retrieves advertisements from the provided data.
 *
 * This function processes the input data and creates Bluetooth advertisements.
 * The returned pointer is platform-specific and should be cast to the
 * appropriate type based on the target platform.
 * Example:
 *
 * @code
 * void *adv_data = hubble_ble_advertise_get(data, data_len, &out_len);
 * @endcode
 *
 * @note - The advertisement packet is a buffer that must be copied and advertised
 *         by the application with Hubble BLE 16-bit service UUID. @see HUBBLE_BLE_UUID
 *       - The payload is encrypted using the key set by @ref hubble_ble_key_set
 *       - Legacy packet type (Extended Advertisements not supported)
 *
 * @param data Pointer to the input data.
 * @param len Length of the input data.
 * @param out_len Advertisement length.
 *
 * @return
 *          - Pointer to the advertisement on success.
 *          - NULL on failure.
 */
void *hubble_ble_advertise_get(const uint8_t *data, size_t len, size_t *out_len);

/**
 * @brief Sets the current UTC time in the Hubble BLE Network.
 *
 * @param utc_time 64-bit integer representing the UTC time.
 *
 * @return
 *          - Pointer to the advertisement on success.
 *          - Non-zero on failure.
 */
int hubble_ble_utc_set(uint64_t utc_time);

/**
 * @brief Sets the encryption key for advertisement data creation.
 *
 * @param key An opaque pointer to the key.
 *
 * @return
 *         - Pointer to the advertisement on success.
 *         - Non-zero on failure.
 */
int hubble_ble_key_set(const void *key);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_BLE_H */
