/* Arduino mbed architectures use mbedtls crypto but include
 * all source files in compilation, so add a guard
 */
#if !defined(ARDUINO_ARCH_MBED)

/* Arduino uses a different header for PSA crypto */
#if defined(ARDUINO)
#  include <PSACrypto.h>
#else // ARDUINO
#  include <psa/crypto.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hubble/ble.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

/* Older verions of Zephyr do not define it */
#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

#define _KEY_BITS_LEN (CONFIG_HUBBLE_KEY_SIZE * BITS_PER_BYTE)

static int _psa_status_to_errno(psa_status_t status)
{
	/* Lets adopt EIO as default error in the lack of something better */
	int ret = -EIO;

	switch (status) {
	case PSA_SUCCESS:
		ret = 0;
		break;
	case PSA_ERROR_INSUFFICIENT_MEMORY:
		ret = -ENOMEM;
		break;
	case PSA_ERROR_INSUFFICIENT_STORAGE:
		ret = -ENOSPC;
		break;
	case PSA_ERROR_COMMUNICATION_FAILURE:
		ret = -EIO;
		break;
	case PSA_ERROR_HARDWARE_FAILURE:
		ret = -EIO;
		break;
	case PSA_ERROR_CORRUPTION_DETECTED:
		ret = -EFAULT;
		break;
	case PSA_ERROR_INSUFFICIENT_ENTROPY:
		ret = -EAGAIN;
		break;
	case PSA_ERROR_STORAGE_FAILURE:
		ret = -EIO;
		break;
	case PSA_ERROR_DATA_INVALID:
		ret = -EINVAL;
		break;
	case PSA_ERROR_DATA_CORRUPT:
		ret = -ENOMSG;
		break;
	default:
		break;
	}

	return ret;
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
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

	status = psa_import_key(&attributes, key, CONFIG_HUBBLE_KEY_SIZE,
				&key_id);
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

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN],
			  const uint8_t *data, size_t len, uint8_t *output)
{
	psa_status_t status;
	psa_key_id_t key_id;
	const psa_algorithm_t alg = PSA_ALG_CTR;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	size_t out_len = 0;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
	psa_set_key_algorithm(&attributes, alg);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attributes, _KEY_BITS_LEN);

	status = psa_import_key(&attributes, key, CONFIG_HUBBLE_KEY_SIZE,
				&key_id);
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

	status = psa_cipher_update(&operation, data, len, output, len, &out_len);
	if (status != PSA_SUCCESS) {
		goto cipher_update_error;
	}

	status = psa_cipher_finish(&operation, output + out_len, len - out_len,
				   &out_len);

cipher_update_error:
cipher_iv_error:
	psa_cipher_abort(&operation);
cipher_setup_error:
	(void)psa_destroy_key(key_id);
import_key_error:
	return status == PSA_SUCCESS ? 0 : -EINVAL;
}

void hubble_crypto_zeroize(void *buf, size_t len)
{
	memset(buf, 0, len);
}

int hubble_crypto_init(void)
{
	psa_status_t status = psa_crypto_init();

	return _psa_status_to_errno(status);
}

#endif // !defined(ARDUINO_ARCH_MBED)
