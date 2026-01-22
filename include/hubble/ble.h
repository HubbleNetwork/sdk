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
 * @{
 */

/**
 * @brief Hubble BLE Network UUID
 *
 * This is the UUID should be listed in the services list.
 */
#define HUBBLE_BLE_UUID 0xFCA6

/**
 * @brief Maximum amount of data sendable in bytes
 *
 * This is the maximum length of data that can be sent with Hubble.
 * If other services or service data are advertised then this number
 * will be smaller for your application given the finite length of
 * advertisements.
 */
#define HUBBLE_BLE_MAX_DATA_LEN 13

/**
 * @brief Retrieves advertisements from the provided data.
 *
 * This function processes the input data and creates the advertisement payload.
 * The returned data should be used with Service Data - 16 bit UUID
 * advertisement type (0x16).
 *
 * It is also required to add Hubble 16-bit service UUID in the complete list of
 * 16-bit service class UUID (0x03).
 *
 * Example:
 *
 * @code
 * int status = hubble_ble_advertise_get(data, data_len, out, &out_len);
 * @endcode
 *
 * The following advertisement packet shows a valid example and where the
 * returned data fits in.
 *
 * | len   | ad type | data   | len                  | ad type | data       |
 * |-------+---------+--------+----------------------+---------+------------|
 * | 0x03  | 0x03    | 0xFCA6 | out_len + 0x01       | 0x16    | ad_data    |
 * |       |         |        | (ad type len)        |         |            |
 * |       |         |        | (out_len is adv_data |         |            |
 * |       |         |        |  len - returned by   |         |            |
 * |       |         |        |  this API)           |         |            |
 *
 *
 * @note - This function is neither thread-safe nor reentrant. The caller must
 *         ensure proper synchronization.
 *       - The payload is encrypted using the key set by @ref hubble_key_set
 *       - Legacy packet type (Extended Advertisements not supported)
 *
 * @param input Pointer to the input data.
 * @param input_len Length of the input data.
 * @param out Output buffer to place data into
 * @param out_len in: Maximum length in out buffer, out: Advertisement length
 *
 * @return
 *          - 0 on success
 *          - Non-zero on failure
 */
int hubble_ble_advertise_get(const uint8_t *input, size_t input_len,
			     uint8_t *out, size_t *out_len);

/**
 * @brief Gets the time remaining until the BLE advertisement expires.
 *
 * Returns the number of milliseconds until the current BLE advertisement
 * expires and a new one should be generated. This helps applications refresh
 * their advertisements in synchronization with the EID rotation period.
 *
 * @return Milliseconds until the current advertisement expires.
 */
uint32_t hubble_ble_advertise_expiration_get(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_BLE_H */
