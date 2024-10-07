#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <psa/crypto.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <hubble_ble.h>
#include <hubble_port.h>

LOG_MODULE_REGISTER(hubbleblenetwork, CONFIG_HUBBLE_BLE_LOG_LEVEL);

/* Older verions of Zephyr do not define it */
#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

#define _KEY_BITS_LEN (HUBBLE_BLE_KEY_LEN * BITS_PER_BYTE)

static int hubble_zephyr_cmac(const uint8_t *key, const uint8_t *input,
			      size_t input_len,
			      uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	psa_status_t status;
	psa_key_id_t key_id;
	size_t mac_length = 0;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
	psa_set_key_algorithm(&attributes, PSA_ALG_CMAC);
	psa_set_key_bits(&attributes, _KEY_BITS_LEN);

	status = psa_import_key(&attributes, key, HUBBLE_BLE_KEY_LEN, &key_id);
	if (status != PSA_SUCCESS) {
		goto import_key_error;
	}

	status = psa_mac_sign_setup(&operation, key_id, PSA_ALG_CMAC);
	if (status != PSA_SUCCESS) {
		goto mac_setup_error;
	}

	status = psa_mac_update(&operation, input, input_len);
	if (status != PSA_SUCCESS) {
		psa_mac_abort(&operation);
		goto mac_update_error;
	}

	status = psa_mac_sign_finish(&operation, output, HUBBLE_AES_BLOCK_SIZE,
				     &mac_length);
mac_update_error:
mac_setup_error:
	psa_destroy_key(key_id);

import_key_error:
	return status == PSA_SUCCESS ? 0 : -EINVAL;
}

static int hubble_zephyr_aes_ctr(
	const uint8_t key[HUBBLE_BLE_KEY_LEN], size_t counter,
	uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN], const uint8_t *data,
	size_t data_len, uint8_t *output)
{
	psa_status_t status;
	psa_key_id_t key_id;
	const psa_algorithm_t alg = PSA_ALG_CTR;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	size_t part_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES);
	size_t len = 0, out_len = 0;

	/* We are limited to 13 bytes in our protocol, which is lower than the
	 * 16 bytes part size. To keep it simple, lets ignore the counter
	 * (because we have to increment it manually in PSA Crypto) and encrypt
	 * only one block (which is enough !!).
	 */
	ARG_UNUSED(counter);

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
	psa_set_key_algorithm(&attributes, alg);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attributes, _KEY_BITS_LEN);

	status = psa_import_key(&attributes, key, HUBBLE_BLE_KEY_LEN, &key_id);
	if (status != PSA_SUCCESS) {
		goto import_key_error;
	}

	status = psa_cipher_encrypt_setup(&operation, key_id, alg);
	if (status != PSA_SUCCESS) {
		goto cipher_setup_error;
	}

	status = psa_cipher_set_iv(&operation, nonce_counter,
				   HUBBLE_BLE_NONCE_BUFFER_LEN);
	if (status != PSA_SUCCESS) {
		goto cipher_iv_error;
	}

	status = psa_cipher_update(&operation, data, data_len, output,
				   part_size, &out_len);
	if (status != PSA_SUCCESS) {
		goto cipher_update_error;
	}

	status = psa_cipher_finish(&operation, output + out_len,
				   part_size - out_len, &len);

cipher_update_error:
cipher_iv_error:
	psa_cipher_abort(&operation);
cipher_setup_error:
	(void)psa_destroy_key(key_id);
import_key_error:
	return status == PSA_SUCCESS ? 0 : -EINVAL;
}

static void hubble_zephyr_zeroize(void *buf, size_t len)
{
	memset(buf, 0, len);
}

const struct hubble_ble_api *hubble_ble_api_get(void)
{
	if (psa_crypto_init() != PSA_SUCCESS) {
		return NULL;
	}

	static struct hubble_ble_api api = {
		.zeroize = hubble_zephyr_zeroize,
		.cmac = hubble_zephyr_cmac,
		.aes_ctr = hubble_zephyr_aes_ctr,
	};

	return &api;
}
