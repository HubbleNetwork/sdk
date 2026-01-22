/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_DECRYPT_H
#define TEST_DECRYPT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parsed BLE advertisement components
 */
struct test_ble_adv_parsed {
	uint16_t service_uuid;
	uint8_t protocol_version;
	uint16_t seq_no;
	uint32_t device_id;
	uint8_t auth_tag[4];
	uint8_t encrypted_data[13];
	size_t encrypted_len;
};

/**
 * @brief Parse advertisement buffer into components
 *
 * @param adv Advertisement data buffer
 * @param len Length of advertisement data
 * @param parsed Output structure with parsed components
 *
 * @return 0 on success, negative errno on failure
 */
int test_parse_ble_adv(const uint8_t *adv, size_t len,
		       struct test_ble_adv_parsed *parsed);

/**
 * @brief Decrypt payload given master key and derived parameters
 *
 * Uses the same key derivation as the production code to decrypt
 * ciphertext back to plaintext.
 *
 * @param master_key The master encryption key (CONFIG_HUBBLE_KEY_SIZE bytes)
 * @param time_counter Time counter (utc_time / 86400000)
 * @param seq_no Sequence number used for key derivation
 * @param ciphertext Encrypted data to decrypt
 * @param cipher_len Length of ciphertext
 * @param plaintext Output buffer for decrypted data (must be >= cipher_len)
 *
 * @return 0 on success, negative errno on failure
 */
int test_decrypt_payload(const uint8_t *master_key, uint32_t time_counter,
			 uint16_t seq_no, const uint8_t *ciphertext,
			 size_t cipher_len, uint8_t *plaintext);

/**
 * @brief Verify CMAC authentication tag
 *
 * @param master_key The master encryption key
 * @param time_counter Time counter (utc_time / 86400000)
 * @param seq_no Sequence number used for key derivation
 * @param ciphertext Encrypted data to authenticate
 * @param cipher_len Length of ciphertext
 * @param expected_tag Expected 4-byte authentication tag
 *
 * @return 0 if tag matches, -EBADMSG if tag mismatch, other negative errno on failure
 */
int test_verify_auth_tag(const uint8_t *master_key, uint32_t time_counter,
			 uint16_t seq_no, const uint8_t *ciphertext,
			 size_t cipher_len, const uint8_t expected_tag[4]);

/**
 * @brief Derive device ID from master key and time counter
 *
 * @param master_key The master encryption key
 * @param time_counter Time counter (utc_time / 86400000)
 * @param device_id Output device ID
 *
 * @return 0 on success, negative errno on failure
 */
int test_derive_device_id(const uint8_t *master_key, uint32_t time_counter,
			  uint32_t *device_id);

/**
 * @brief Derive encryption key from master key and parameters
 *
 * @param master_key The master encryption key
 * @param time_counter Time counter (utc_time / 86400000)
 * @param seq_no Sequence number
 * @param enc_key Output encryption key (CONFIG_HUBBLE_KEY_SIZE bytes)
 *
 * @return 0 on success, negative errno on failure
 */
int test_derive_encryption_key(const uint8_t *master_key, uint32_t time_counter,
			       uint16_t seq_no, uint8_t *enc_key);

/**
 * @brief Derive nonce from master key and parameters
 *
 * @param master_key The master encryption key
 * @param time_counter Time counter (utc_time / 86400000)
 * @param seq_no Sequence number
 * @param nonce Output nonce (12 bytes)
 *
 * @return 0 on success, negative errno on failure
 */
int test_derive_nonce(const uint8_t *master_key, uint32_t time_counter,
		      uint16_t seq_no, uint8_t nonce[12]);

#ifdef __cplusplus
}
#endif

#endif /* TEST_DECRYPT_H */
