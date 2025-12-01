/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_H
#define INCLUDE_HUBBLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble Network Function APIs
 * @defgroup hubble_api Network Function APIs
 * @since 0.1
 * @version 0.1
 * @{
 */

/**
 * @brief Maximum amount of data sendable in bytes
 *
 * This is the maximum length of data that can be sent with Hubble.
 * If other services or service data are advertised then this number
 * will be smaller for your application given the finite length of
 * advertisements.
 */
#define HUBBLE_MAX_USER_DATA_LEN 13

/**
 * @brief Initializes the Hubble function with the given UTC time.
 *
 * Calling this function is essential before using @ref hubble_ble_advertise_get
 *
 * @code
 * uint64_t current_utc_time = 1633072800000; // Example UTC time in milliseconds
 * static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE] = {...};
 * int ret = hubble_ble_init(current_utc_time, master_key);
 * @endcode
 *
 * @param utc_time The UTC time in milliseconds since the Unix epoch (January 1, 1970).
 *                 Set to 0 to set later via hubble_ble_utc_set
 * @param key An opaque pointer to the key. If NULL, must be set with hubble_ble_key_set
 *            before getting advertisements.
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_init(uint64_t utc_time, const void *key);

/**
 * @brief Deinitialize the Hubble functionality.
 */
int hubble_deinit(void);

/**
 * @brief Sets the current UTC time in the Hubble BLE Network.
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

#ifdef CONFIG_HUBBLE_BLE_NETWORK

/**
 * @brief Hubble BLE Network UUID
 *
 * This is the UUID should be listed in the services list.
 */
#define HUBBLE_BLE_UUID 0xFCA6

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
 *       - The payload is encrypted using the key set by @ref hubble_ble_key_set
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
int hubble_ble_generate_adv_service_data(
    const uint8_t *input, size_t input_len, uint8_t *out, size_t *out_len);

/**
 * @brief The time until the advertisement payload needs to be updated.
 *
 * The BLE advertisement needs to be updated periodically, this gets the
 * timeout in milliseconds until the next time the advertisement needs to be
 * updated.
 *
 * @return
 *  - Milliseconds until an update is necessary (0 if immediately)
 */
uint32_t hubble_ble_get_adv_update_timeout_ms(void);

#endif // CONFIG_HUBBLE_BLE_NETWORK

#ifdef CONFIG_HUBBLE_SAT_NETWORK

/**
 * @brief Initializes the Hubble satellite system.
 *
 * This function performs the necessary setup and initialization
 * for the Hubble satellite system. It ensures that all required
 * components are properly configured and ready for operation.
 *
 * @return int
 *   - 0 on successful initialization.
 *   - Negative value on failure, indicating the error code.
 *
 * @note This function must be called before any other Hubble satellite
 *       operations are performed.
 *
 * @warning Ensure that the system is in a safe state before calling
 *          this function to avoid unexpected behavior.
 */
int hubble_sat_init(void);

/**
 * @brief Enable the hardware needed for satellite transmissions.
 *
 * This function powers on the radio hardware and prepares it for communication.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int hubble_sat_enable(void);

/**
 * @brief Disable the hardware needed for satellite transmissions.
 *
 * This function powers off the radio hardware to save power when it
 * is not in use.
 */
void hubble_sat_disable(void);

/**
 * @brief Transmit a packet using the Hubble satellite communication system.
 *
 * This function sends a packet over the satellite communication channel.
 * The packet must be properly formatted and adhere to the Hubble protocol.
 *
 * @param packet A pointer to the ~hubble_packet~ structure containing the
 *               data to be transmitted.
 *
 * @return 0 on successful transmission, or a negative error code on failure.
 *
 * @note Ensure that the satellite hardware is enabled using ~hubble_sat_enable~
 *       before calling this function.
 *
 * @warning This function does not perform any validation on the packet
 *          structure. It is the caller's responsibility to ensure the
 *          packet is correctly formatted.
 */
int hubble_sat_transmit(const void *payload, size_t length);

#endif // CONFIG_HUBBLE_SAT_NETWORK

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_BLE_H */
