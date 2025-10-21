#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <mbedtls/aes.h>
#include <mbedtls/cmac.h>

#include <hubble/ble.h>
#include <hubble/hubble_port.h>

/* Older verions of Zephyr do not define it */
#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

#define _KEY_BITS_LEN               (CONFIG_HUBBLE_KEY_SIZE * BITS_PER_BYTE)

#define HUBBLE_BLE_STREAM_BLOCK_LEN 16


static int hubble_zephyr_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			      const uint8_t *input, size_t input_len,
			      uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	int ret;
	mbedtls_cipher_context_t ctx;
	const mbedtls_cipher_info_t *cipher_info;

	/* Initialize CMAC context */
	mbedtls_cipher_init(&ctx);

	/* Get cipher info for AES-256 ECB (Electronic Codebook) mode */
	cipher_info = mbedtls_cipher_info_from_type(
					COND_CODE_1(CONFIG_HUBBLE_NETWORK_KEY_256,
					    (MBEDTLS_CIPHER_AES_256_ECB),
					    (MBEDTLS_CIPHER_AES_128_ECB)));
	if (cipher_info == NULL) {
		ret = MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
		goto exit;
	}

	/* Setup the CMAC context with AES-256 ECB */
	ret = mbedtls_cipher_setup(&ctx, cipher_info);
	if (ret) {
		goto exit;
	}

	ret = mbedtls_cipher_cmac_starts(&ctx, key, _KEY_BITS_LEN);
	if (ret) {
		goto exit;
	}

	/* Update the CMAC with the input message */
	ret = mbedtls_cipher_cmac_update(&ctx, input, input_len);
	if (ret != 0) {
		goto exit;
	}

	ret = mbedtls_cipher_cmac_finish(&ctx, output);
	if (ret != 0) {
		goto exit;
	}

exit:
	mbedtls_cipher_free(&ctx);
	return ret;
}

static int hubble_zephyr_aes_ctr(
	const uint8_t key[CONFIG_HUBBLE_KEY_SIZE], size_t counter,
	uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN], const uint8_t *data,
	size_t len, uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	int ret;
	mbedtls_aes_context aes_ctx;
	uint8_t stream_block[HUBBLE_BLE_STREAM_BLOCK_LEN] = {0};

	/* Initialize AES context */
	mbedtls_aes_init(&aes_ctx);

	/* Set the AES key */
	ret = mbedtls_aes_setkey_enc(&aes_ctx, key, _KEY_BITS_LEN);
	if (ret) {
		goto key_error;
	}

	ret = mbedtls_aes_crypt_ctr(&aes_ctx, len, &counter, nonce_counter,
				    stream_block, data, output);
	if (ret != 0) {
		goto crypt_ctr_error;
	}

crypt_ctr_error:
	mbedtls_platform_zeroize(stream_block, sizeof(stream_block));
key_error:
	mbedtls_aes_free(&aes_ctx);

	return ret;
}

const struct hubble_ble_api *hubble_ble_api_get(void)
{
	static struct hubble_ble_api api = {
		.zeroize = mbedtls_platform_zeroize,
		.cmac = hubble_zephyr_cmac,
		.aes_ctr = hubble_zephyr_aes_ctr,
	};

	return &api;
}
