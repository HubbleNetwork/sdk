/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_PORT_CRYPTO_H
#define INCLUDE_HUBBLE_PORT_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Hubble Crypto API.
 *
 * Cryptographic functions that need to be implemented by
 * a crypto provider.
 *
 * @defgroup hubble_crypto Hubble Network Crypto APIs
 * @since 0.1
 * @version 0.1
 *
 * @{
 */

/**
 * @brief Securely clear out a memory region.
 *
 * This function is used to securely overwrite a memory buffer
 * with zeros, ensuring that sensitive data is erased.
 *
 * @param buf Pointer to the input data.
 * @param len Length of the input data.
 */
void hubble_crypto_zeroize(void *buf, size_t len);

/**
 * @brief Cryptographic initialization.
 *
 * This function must be called before any other cryptographic function
 * otherwise the behavior is undefined.
 *
 * @return 0 on success, non-zero on error.
 */
int hubble_crypto_init(void);

/**
 * @brief Perform AES encryption in Counter (CTR) mode.
 *
 * @param key A pointer to the encryption key (size: CONFIG_HUBBLE_KEY_SIZE).
 * @param nonce_counter A pointer to the nonce and counter buffer (size:
 *                      HUBBLE_BLE_NONCE_BUFFER_LEN).
 * @param data A pointer to the input data buffer to be encrypted.
 * @param len The length of the input data in bytes.
 * @param output A pointer to the output buffer where the encrypted data will be
 *               stored. It must be at least the size of the input data in bytes.
 *
 * @return Returns 0 on success, or a non-zero error code on failure.
 */
int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN],
			  const uint8_t *data, size_t len, uint8_t *output);

/**
 * @brief Computes the Cipher-based Message Authentication Code (CMAC).
 *
 * @param key The secret key used for CMAC calculation. The size of the
 *            key is defined by CONFIG_HUBBLE_KEY_SIZE.
 * @param data Pointer to the input data for which the CMAC is
 *             calculated.
 * @param len The length of the input data in bytes.
 * @param output Pointer to the buffer where the CMAC (message
 *               authentication code) will be stored. This buffer must
 *               be large enough to hold the CMAC value
 *              (typically the size of the AES block, 16 bytes).
 *
 * @return 0 on success, non-zero on error.
 */
int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *data, size_t len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE]);

/**
 * @}
 */ /* hubble_crypto */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_PORT_CRYPTO_H */
