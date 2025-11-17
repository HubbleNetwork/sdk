#include <tinycrypt/aes.h>
#include <tinycrypt/cmac_mode.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/ctr_mode.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

void hubble_crypto_zeroize(void *buf, size_t len)
{
	memset(buf, 0, len);
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	int ret;
	struct tc_cmac_struct state;
	struct tc_aes_key_sched_struct sched;

	ret = tc_cmac_setup(&state, key, &sched);
	if (ret != TC_CRYPTO_SUCCESS) {
		return -ENODATA;
	}

	ret = tc_cmac_init(&state);
	if (ret != TC_CRYPTO_SUCCESS) {
		goto init_error;
	}

	ret = tc_cmac_update(&state, input, input_len);
	if (ret != TC_CRYPTO_SUCCESS) {
		goto cmac_error;
	}

	ret = tc_cmac_final(output, &state);
	if (ret != TC_CRYPTO_SUCCESS) {
		goto cmac_error;
	}

	tc_cmac_erase(&state);

	return 0;

cmac_error:
	hubble_crypto_zeroize(output, HUBBLE_AES_BLOCK_SIZE);

init_error:
	tc_cmac_erase(&state);
	return -ENODATA;
}

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN],
			  const uint8_t *data, size_t data_len, uint8_t *output)
{
	int ret;
	uint8_t ctr;
	uint8_t out[HUBBLE_BLE_NONCE_BUFFER_LEN + HUBBLE_AES_BLOCK_SIZE];
	struct tc_aes_key_sched_struct sched;

	if (data_len == 0) {
		return 0;
	}

	memcpy(out, nonce_counter, HUBBLE_BLE_NONCE_BUFFER_LEN);

	ret = tc_aes128_set_encrypt_key(&sched, key);
	if (ret != TC_CRYPTO_SUCCESS) {
		goto err;
	}

	if (tc_ctr_mode(&out[HUBBLE_BLE_NONCE_BUFFER_LEN], data_len, data,
			data_len, &ctr, &sched) == 0) {
		goto err;
	}
	memcpy(output, &out[HUBBLE_BLE_NONCE_BUFFER_LEN], data_len);

err:
	hubble_crypto_zeroize(out, sizeof(out));
	return ret == TC_CRYPTO_SUCCESS ? 0 : -ENODATA;
}

int hubble_crypto_init(void)
{
	return 0;
}
