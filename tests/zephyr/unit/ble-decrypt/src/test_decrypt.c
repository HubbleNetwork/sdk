/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_decrypt.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <hubble/ble.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

#define BITS_PER_BYTE        8
#define BLE_CONTEXT_LEN      12
#define BLE_MESSAGE_LEN      64
#define BLE_NONCE_LEN        12
#define BLE_AUTH_TAG_SIZE    4
#define BLE_ADVERTISE_PREFIX 2
#define BLE_ADDR_SIZE        6

/* Big-endian conversion helper */
#define CPU_TO_BE32(x)                                                         \
	((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) |              \
	 (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))

/**
 * @brief KBKDF Counter mode implementation (mirrors hubble_ble.c)
 *
 * This is a direct port of the _kbkdf_counter function from hubble_ble.c
 * to enable test code to derive the same keys.
 */
static int _test_kbkdf_counter(const uint8_t *key, const char *label,
			       size_t label_len, const uint8_t *context,
			       size_t context_len, uint8_t *output, size_t olen)
{
	int ret = 0;
	uint8_t prf_output[HUBBLE_AES_BLOCK_SIZE];
	uint8_t message[BLE_MESSAGE_LEN];
	uint32_t counter = 1U;
	uint32_t total = 0U;
	uint8_t separation_byte = 0x00;

	/* Message format: Counter + Label + Context + Length (in bits) */
	uint32_t message_length = sizeof(counter) + label_len +
				  sizeof(separation_byte) + context_len +
				  sizeof(uint32_t);

	/* Check for message length overflow */
	if (message_length >= sizeof(message)) {
		ret = -EINVAL;
		goto exit;
	}

	/* Copy label after the counter */
	memcpy((message + sizeof(counter)), label, label_len);
	/* Separation byte (as defined by the standard) */
	message[sizeof(counter) + label_len] = separation_byte;
	/* Copy the context */
	memcpy((message + sizeof(counter) + label_len + sizeof(separation_byte)),
	       context, context_len);
	/* Length in bits at the end */
	memcpy((message + sizeof(counter) + label_len +
		sizeof(separation_byte) + context_len),
	       (uint8_t *)&(uint32_t){CPU_TO_BE32(olen * BITS_PER_BYTE)},
	       sizeof(uint32_t));

	while (total < olen) {
		size_t remaining = olen - total;

		/* Insert counter into the message */
		memcpy(message, (uint8_t *)&(uint32_t){CPU_TO_BE32(counter)},
		       sizeof(counter));

		/* Perform AES-CMAC with the key and the prepared message */
		ret = hubble_crypto_cmac(key, message, message_length,
					 prf_output);
		if (ret != 0) {
			goto exit;
		}

		/* Copy the output */
		if (remaining > HUBBLE_AES_BLOCK_SIZE) {
			remaining = HUBBLE_AES_BLOCK_SIZE;
		}

		memcpy(output + total, prf_output, remaining);
		total += remaining;
		counter++;
	}

exit:
	/* Clear sensitive information */
	hubble_crypto_zeroize(prf_output, sizeof(prf_output));
	hubble_crypto_zeroize(message, sizeof(message));

	return ret;
}

/**
 * @brief Get derived key for a specific label
 */
static int _test_derived_key_get(const uint8_t *master_key, const char *label,
				 uint32_t counter, uint8_t *output_key)
{
	uint8_t context[BLE_CONTEXT_LEN] = {0};

	snprintf((char *)context, BLE_CONTEXT_LEN, "%" PRIu32, counter);

	return _test_kbkdf_counter(master_key, label, strlen(label), context,
				   strlen((const char *)context), output_key,
				   CONFIG_HUBBLE_KEY_SIZE);
}

/**
 * @brief Get derived value using two-stage key derivation
 */
static int _test_derived_value_get(
	const uint8_t *master_key, const char *key_label,
	const char *value_label, uint32_t time_counter, uint16_t seq_no,
	uint8_t *output_value, uint32_t output_len)
{
	int ret;
	uint8_t context[BLE_CONTEXT_LEN] = {0};
	uint8_t derived_key[CONFIG_HUBBLE_KEY_SIZE] = {0};

	/* First stage: derive key from master key */
	ret = _test_derived_key_get(master_key, key_label, time_counter,
				    derived_key);
	if (ret != 0) {
		goto exit;
	}

	/* Second stage: derive value from derived key */
	snprintf((char *)context, BLE_CONTEXT_LEN, "%u", seq_no);
	ret = _test_kbkdf_counter(derived_key, value_label, strlen(value_label),
				  context, strlen((const char *)context),
				  output_value, output_len);

exit:
	hubble_crypto_zeroize(derived_key, sizeof(derived_key));
	return ret;
}

int test_parse_ble_adv(const uint8_t *adv, size_t len,
		       struct test_ble_adv_parsed *parsed)
{
	if (adv == NULL || parsed == NULL) {
		return -EINVAL;
	}

	/* Minimum length: UUID (2) + addr (6) + auth_tag (4) = 12 bytes */
	if (len < BLE_ADVERTISE_PREFIX + BLE_ADDR_SIZE + BLE_AUTH_TAG_SIZE) {
		return -EINVAL;
	}

	memset(parsed, 0, sizeof(*parsed));

	/* Service UUID (little-endian) */
	parsed->service_uuid = adv[0] | (adv[1] << 8);

	/* Protocol version (6 bits) and seq_no (10 bits) from addr */
	parsed->protocol_version = (adv[2] >> 2) & 0x3F;
	parsed->seq_no = ((adv[2] & 0x03) << 8) | adv[3];

	/* Device ID (4 bytes, little-endian) */
	memcpy(&parsed->device_id, &adv[4], sizeof(parsed->device_id));

	/* Auth tag (4 bytes) */
	memcpy(parsed->auth_tag, &adv[8], BLE_AUTH_TAG_SIZE);

	/* Encrypted data */
	parsed->encrypted_len =
		len - BLE_ADVERTISE_PREFIX - BLE_ADDR_SIZE - BLE_AUTH_TAG_SIZE;
	if (parsed->encrypted_len > 0) {
		memcpy(parsed->encrypted_data, &adv[12], parsed->encrypted_len);
	}

	return 0;
}

int test_decrypt_payload(const uint8_t *master_key, uint32_t time_counter,
			 uint16_t seq_no, const uint8_t *ciphertext,
			 size_t cipher_len, uint8_t *plaintext)
{
	int ret;
	uint8_t encryption_key[CONFIG_HUBBLE_KEY_SIZE] = {0};
	uint8_t nonce[HUBBLE_BLE_NONCE_BUFFER_LEN] = {0};

	if (master_key == NULL || plaintext == NULL) {
		return -EINVAL;
	}

	if (cipher_len == 0) {
		return 0;
	}

	if (ciphertext == NULL) {
		return -EINVAL;
	}

	/* Derive encryption key */
	ret = _test_derived_value_get(master_key, "EncryptionKey", "Key",
				      time_counter, seq_no, encryption_key,
				      sizeof(encryption_key));
	if (ret != 0) {
		goto exit;
	}

	/* Derive nonce */
	ret = _test_derived_value_get(master_key, "NonceKey", "Nonce",
				      time_counter, seq_no, nonce, BLE_NONCE_LEN);
	if (ret != 0) {
		goto exit;
	}

	/* AES-CTR is symmetric - encrypt and decrypt are the same operation */
	ret = hubble_crypto_aes_ctr(encryption_key, nonce, ciphertext,
				    cipher_len, plaintext);

exit:
	hubble_crypto_zeroize(encryption_key, sizeof(encryption_key));
	hubble_crypto_zeroize(nonce, sizeof(nonce));
	return ret;
}

int test_verify_auth_tag(const uint8_t *master_key, uint32_t time_counter,
			 uint16_t seq_no, const uint8_t *ciphertext,
			 size_t cipher_len, const uint8_t expected_tag[4])
{
	int ret;
	uint8_t encryption_key[CONFIG_HUBBLE_KEY_SIZE] = {0};
	uint8_t computed_tag[HUBBLE_AES_BLOCK_SIZE] = {0};

	if (master_key == NULL || expected_tag == NULL) {
		return -EINVAL;
	}

	/* Derive encryption key (CMAC uses the same key) */
	ret = _test_derived_value_get(master_key, "EncryptionKey", "Key",
				      time_counter, seq_no, encryption_key,
				      sizeof(encryption_key));
	if (ret != 0) {
		goto exit;
	}

	/* Compute CMAC over ciphertext */
	ret = hubble_crypto_cmac(encryption_key, ciphertext, cipher_len,
				 computed_tag);
	if (ret != 0) {
		goto exit;
	}

	/* Compare first 4 bytes of CMAC with expected tag */
	if (memcmp(computed_tag, expected_tag, BLE_AUTH_TAG_SIZE) != 0) {
		ret = -EBADMSG;
	}

exit:
	hubble_crypto_zeroize(encryption_key, sizeof(encryption_key));
	hubble_crypto_zeroize(computed_tag, sizeof(computed_tag));
	return ret;
}

int test_derive_device_id(const uint8_t *master_key, uint32_t time_counter,
			  uint32_t *device_id)
{
	uint8_t device_id_bytes[4] = {0};
	int ret;

	if (master_key == NULL || device_id == NULL) {
		return -EINVAL;
	}

	ret = _test_derived_value_get(master_key, "DeviceKey", "DeviceID",
				      time_counter, 0, device_id_bytes,
				      sizeof(device_id_bytes));
	if (ret != 0) {
		return ret;
	}

	/* Device ID is stored as little-endian in the packet */
	memcpy(device_id, device_id_bytes, sizeof(*device_id));

	return 0;
}

int test_derive_encryption_key(const uint8_t *master_key, uint32_t time_counter,
			       uint16_t seq_no, uint8_t *enc_key)
{
	if (master_key == NULL || enc_key == NULL) {
		return -EINVAL;
	}

	return _test_derived_value_get(master_key, "EncryptionKey", "Key",
				       time_counter, seq_no, enc_key,
				       CONFIG_HUBBLE_KEY_SIZE);
}

int test_derive_nonce(const uint8_t *master_key, uint32_t time_counter,
		      uint16_t seq_no, uint8_t nonce[12])
{
	if (master_key == NULL || nonce == NULL) {
		return -EINVAL;
	}

	return _test_derived_value_get(master_key, "NonceKey", "Nonce",
				       time_counter, seq_no, nonce,
				       BLE_NONCE_LEN);
}
